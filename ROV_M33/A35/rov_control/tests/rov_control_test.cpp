#include "test_support.hpp"

#include "fake_transport.hpp"
#include "internal/rov_control_test_access.hpp"

#include <chrono>
#include <memory>
#include <string>

using namespace std::chrono_literals;

namespace {

struct ControlFixture {
    std::unique_ptr<rov::RovControl> control;
    FakeTransport* transport{nullptr};
};

ControlFixture makeControl()
{
    auto transport = std::make_unique<FakeTransport>();
    FakeTransport* raw = transport.get();
    auto control = rov::internal::RovControlTestAccess::create(
        std::move(transport));
    TEST_CHECK(control->open());
    return {std::move(control), raw};
}

} // namespace

void runRovControlTests(TestSuite& suite)
{
    suite.run("RovControl setServo canonical payload", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) + " ok\r\n");
            });
        TEST_CHECK(fixture.control->setServo(9U, 30U));
        const auto commands = fixture.transport->commands();
        TEST_CHECK(commands.size() == 1U);
        TEST_CHECK(commands[0] == "0000 set servo 9 30");
    });

    suite.run("RovControl invalid servo id does not send", [] {
        auto fixture = makeControl();
        const auto result = fixture.control->setServo(10U, 30U);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::BadArgument);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::Client);
        TEST_CHECK(fixture.transport->commands().empty());
    });

    suite.run("RovControl invalid angle does not send", [] {
        auto fixture = makeControl();
        const auto result = fixture.control->setServo(9U, 181U);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::BadArgument);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::Client);
        TEST_CHECK(fixture.transport->commands().empty());
    });

    suite.run("RovControl readDyp preserves 65533", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                    " ok dyp distance_mm 65533\r\n");
            });
        const auto result = fixture.control->readDyp();
        TEST_CHECK(result);
        TEST_CHECK(result.value->distanceMm == 65533U);
        TEST_CHECK(commandPayload(fixture.transport->commands()[0]) ==
                   "sensor dyp");
    });

    suite.run("RovControl stop canonical command", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) + " ok\r\n");
            });
        TEST_CHECK(fixture.control->stop());
        TEST_CHECK(commandPayload(fixture.transport->commands()[0]) == "stop");
    });

    suite.run("RovControl move canonical command", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) + " ok\r\n");
            });
        TEST_CHECK(fixture.control->move());
        TEST_CHECK(commandPayload(fixture.transport->commands()[0]) == "move");
    });
}
