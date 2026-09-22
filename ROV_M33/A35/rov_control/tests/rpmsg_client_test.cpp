#include "test_support.hpp"

#include "fake_transport.hpp"
#include "internal/rpmsg_client.hpp"
#include "internal/sequence_allocator.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;

namespace {

struct ClientFixture {
    std::unique_ptr<rov::internal::RpmsgClient> client;
    FakeTransport* transport{nullptr};
};

ClientFixture makeClient(std::chrono::milliseconds timeout = 250ms,
                         std::uint16_t initialSequence = 0)
{
    auto transport = std::make_unique<FakeTransport>();
    FakeTransport* raw = transport.get();
    auto client = std::make_unique<rov::internal::RpmsgClient>(
        std::move(transport), timeout, 5000ms, initialSequence);
    const auto opened = client->open();
    TEST_CHECK(opened);
    return {std::move(client), raw};
}

void setAutomaticAck(FakeTransport& transport)
{
    transport.setCommandHandler([](const std::string& command,
                                   FakeTransport& fake) {
        fake.enqueueRead(commandSequence(command) + " ok\r\n");
    });
}

} // namespace

void runRpmsgClientTests(TestSuite& suite)
{
    suite.run("client open failure is TransportIo", [] {
        auto transport = std::make_unique<FakeTransport>();
        transport->setFailOpen(true);
        rov::internal::RpmsgClient client(std::move(transport));
        const auto result = client.open();
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::TransportIo);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::Client);
    });

    suite.run("client normal request", [] {
        auto fixture = makeClient();
        setAutomaticAck(*fixture.transport);
        const auto result = fixture.client->request(
            "stop", rov::internal::ExpectedResponseKind::Ack);
        TEST_CHECK(result);
        TEST_CHECK(result.sequence == 0U);
        const auto commands = fixture.transport->commands();
        TEST_CHECK(commands.size() == 1U);
        TEST_CHECK(commands[0] == "0000 stop");
    });

    suite.run("client delayed DYP", [] {
        auto fixture = makeClient();
        auto future = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "sensor dyp", rov::internal::ExpectedResponseKind::Dyp);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(1U, 200ms));
        fixture.transport->enqueueRead(
            "0000 ok dyp distance_mm 65533\r\n");
        const auto result = future.get();
        TEST_CHECK(result);
        TEST_CHECK(result.value->dypReading->distanceMm == 65533U);
    });

    suite.run("client out-of-order concurrent responses", [] {
        auto fixture = makeClient();
        auto dyp = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "sensor dyp", rov::internal::ExpectedResponseKind::Dyp);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(1U, 200ms));
        auto servo = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "set servo 9 30", rov::internal::ExpectedResponseKind::Ack);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(2U, 200ms));
        const auto commands = fixture.transport->commands();
        std::string dypSequence;
        std::string servoSequence;
        for (const auto& command : commands) {
            if (commandPayload(command) == "sensor dyp") {
                dypSequence = commandSequence(command);
            } else if (commandPayload(command) == "set servo 9 30") {
                servoSequence = commandSequence(command);
            }
        }
        TEST_CHECK(!dypSequence.empty());
        TEST_CHECK(!servoSequence.empty());
        fixture.transport->enqueueRead(servoSequence + " ok\r\n");
        fixture.transport->enqueueRead(
            dypSequence + " ok dyp distance_mm 272\r\n");
        TEST_CHECK(servo.get());
        const auto dypResult = dyp.get();
        TEST_CHECK(dypResult);
        TEST_CHECK(dypResult.value->dypReading->distanceMm == 272U);
    });

    suite.run("client partial read", [] {
        auto fixture = makeClient();
        auto future = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "stop", rov::internal::ExpectedResponseKind::Ack);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(1U, 200ms));
        fixture.transport->enqueueRead("0000 o");
        fixture.transport->enqueueRead("k\r\n");
        TEST_CHECK(future.get());
    });

    suite.run("client multiple lines in one read", [] {
        auto fixture = makeClient();
        auto first = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "stop", rov::internal::ExpectedResponseKind::Ack);
        });
        auto second = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "move", rov::internal::ExpectedResponseKind::Ack);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(2U, 200ms));
        const auto commands = fixture.transport->commands();
        std::string responses;
        for (const auto& command : commands) {
            responses += commandSequence(command) + " ok\r\n";
        }
        fixture.transport->enqueueRead(responses);
        TEST_CHECK(first.get());
        TEST_CHECK(second.get());
    });

    suite.run("client local timeout", [] {
        auto fixture = makeClient(40ms);
        const auto result = fixture.client->request(
            "sensor dyp", rov::internal::ExpectedResponseKind::Dyp);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::Timeout);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::Client);
    });

    suite.run("client M33 timeout", [] {
        auto fixture = makeClient();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                                 " err timeout\r\n");
            });
        const auto result = fixture.client->request(
            "sensor dyp", rov::internal::ExpectedResponseKind::Dyp);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::Timeout);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::M33);
    });

    suite.run("client late response is discarded", [] {
        auto fixture = makeClient(40ms);
        const auto timedOut = fixture.client->request(
            "sensor dyp", rov::internal::ExpectedResponseKind::Dyp);
        TEST_CHECK(!timedOut);
        fixture.transport->enqueueRead(
            "0000 ok dyp distance_mm 111\r\n");
        std::this_thread::sleep_for(20ms);

        auto next = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "stop", rov::internal::ExpectedResponseKind::Ack, 250ms);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(2U, 200ms));
        const auto commands = fixture.transport->commands();
        TEST_CHECK(commandSequence(commands[1]) == "0001");
        fixture.transport->enqueueRead("0001 ok\r\n");
        TEST_CHECK(next.get());
    });

    suite.run("client 9999 wraps to 0000", [] {
        auto fixture = makeClient(250ms, 9999U);
        setAutomaticAck(*fixture.transport);
        TEST_CHECK(fixture.client->request(
            "stop", rov::internal::ExpectedResponseKind::Ack));
        TEST_CHECK(fixture.client->request(
            "move", rov::internal::ExpectedResponseKind::Ack));
        const auto commands = fixture.transport->commands();
        TEST_CHECK(commandSequence(commands[0]) == "9999");
        TEST_CHECK(commandSequence(commands[1]) == "0000");
    });

    suite.run("sequence allocator skips pending", [] {
        rov::internal::SequenceAllocator allocator(42U);
        const std::unordered_set<std::uint16_t> occupied{42U, 43U, 44U};
        const auto selected = allocator.select([&occupied](std::uint16_t seq) {
            return occupied.find(seq) != occupied.end();
        });
        TEST_CHECK(selected == 45U);
    });

    suite.run("sequence allocator reports exhausted", [] {
        rov::internal::SequenceAllocator allocator;
        const auto selected = allocator.select([](std::uint16_t) {
            return true;
        });
        TEST_CHECK(!selected);
    });

    suite.run("client unsolicited banner does not complete request", [] {
        auto fixture = makeClient();
        auto future = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "stop", rov::internal::ExpectedResponseKind::Ack);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(1U, 200ms));
        fixture.transport->enqueueRead("ACTUATOR I2C4 READY\r\n");
        TEST_CHECK(future.wait_for(20ms) == std::future_status::timeout);
        fixture.transport->enqueueRead("0000 ok\r\n");
        TEST_CHECK(future.get());
    });

    suite.run("client malformed unsequenced line does not complete request", [] {
        auto fixture = makeClient();
        auto future = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "stop", rov::internal::ExpectedResponseKind::Ack);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(1U, 200ms));
        fixture.transport->enqueueRead("00A0 ok\r\n");
        TEST_CHECK(future.wait_for(20ms) == std::future_status::timeout);
        fixture.transport->enqueueRead("0000 ok\r\n");
        TEST_CHECK(future.get());
    });

    suite.run("client disconnect fails pending request", [] {
        auto fixture = makeClient();
        auto future = std::async(std::launch::async, [&fixture] {
            return fixture.client->request(
                "sensor dyp", rov::internal::ExpectedResponseKind::Dyp);
        });
        TEST_CHECK(fixture.transport->waitForCommandCount(1U, 200ms));
        fixture.transport->disconnect();
        const auto result = future.get();
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::Disconnected);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::Client);
    });

    suite.run("transport handles EINTR and partial write", [] {
        auto fixture = makeClient();
        fixture.transport->setRetryWrites(1U);
        fixture.transport->setMaxWriteChunk(3U);
        setAutomaticAck(*fixture.transport);
        const auto result = fixture.client->request(
            "set servo 9 30", rov::internal::ExpectedResponseKind::Ack);
        TEST_CHECK(result);
        TEST_CHECK(fixture.transport->writeCallCount() > 2U);
        const auto commands = fixture.transport->commands();
        TEST_CHECK(commands.size() == 1U);
        TEST_CHECK(commands[0] == "0000 set servo 9 30");
    });

    suite.run("client fatal write error is TransportIo", [] {
        auto fixture = makeClient();
        fixture.transport->setFailWrites(true);
        const auto result = fixture.client->request(
            "stop", rov::internal::ExpectedResponseKind::Ack);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::TransportIo);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::Client);
        TEST_CHECK(fixture.transport->commands().empty());
    });
}
