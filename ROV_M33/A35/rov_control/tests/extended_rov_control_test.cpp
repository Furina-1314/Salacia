#include "test_support.hpp"

#include "fake_transport.hpp"
#include "internal/rov_control_test_access.hpp"

#include <memory>
#include <string>

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

void replyOk(const std::string& command, FakeTransport& fake)
{
    fake.enqueueRead(commandSequence(command) + " ok\r\n");
}

void checkLastPayload(const ControlFixture& fixture,
                      const std::string& expected)
{
    const auto commands = fixture.transport->commands();
    TEST_CHECK(!commands.empty());
    TEST_CHECK(commandPayload(commands.back()) == expected);
}

} // namespace

void runExtendedRovControlTests(TestSuite& suite)
{
    suite.run("RovControl servo all and mid canonical commands", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(replyOk);
        TEST_CHECK(fixture.control->setAllServos(90U));
        checkLastPayload(fixture, "set servo all 90");
        TEST_CHECK(fixture.control->centerServo(3U));
        checkLastPayload(fixture, "set servo 3 mid");
        TEST_CHECK(fixture.control->centerAllServos());
        checkLastPayload(fixture, "set servo all mid");
    });

    suite.run("RovControl servo local validation sends nothing", [] {
        auto fixture = makeControl();
        TEST_CHECK(!fixture.control->setAllServos(181U));
        TEST_CHECK(!fixture.control->centerServo(10U));
        TEST_CHECK(!fixture.control->getServo(10U));
        TEST_CHECK(fixture.transport->commands().empty());
    });

    suite.run("RovControl get servo one and all", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                const auto payload = commandPayload(command);
                const std::string result = payload == "get servo 3"
                    ? " ok servo 3 91\r\n"
                    : " ok servo all 0 10 20 30 40 50 60 70 80 90\r\n";
                fake.enqueueRead(commandSequence(command) + result);
            });
        const auto one = fixture.control->getServo(3U);
        TEST_CHECK(one && *one.value == 91U);
        const auto all = fixture.control->getAllServos();
        TEST_CHECK(all && (*all.value)[9] == 90U);
        TEST_CHECK(fixture.transport->commands().size() == 2U);
    });

    suite.run("RovControl get servo rejects mismatched response id", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                                 " ok servo 4 90\r\n");
            });
        const auto result = fixture.control->getServo(3U);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::ProtocolError);
    });

    suite.run("RovControl propeller setters canonical payloads", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(replyOk);
        TEST_CHECK(fixture.control->setVerticalBase(-10));
        checkLastPayload(fixture, "set propeller vertical base -10");
        TEST_CHECK(fixture.control->setVerticalPropeller(13U, 100));
        checkLastPayload(fixture, "set propeller vertical 13 100");
        TEST_CHECK(fixture.control->setHorizontalBase(5));
        checkLastPayload(fixture, "set propeller horizontal base 5");
        TEST_CHECK(fixture.control->setHorizontalPropeller(14U, -100));
        checkLastPayload(fixture, "set propeller horizontal 14 -100");
    });

    suite.run("RovControl propeller invalid arguments do not send", [] {
        auto fixture = makeControl();
        TEST_CHECK(!fixture.control->setVerticalBase(101));
        TEST_CHECK(!fixture.control->setVerticalPropeller(14U, 0));
        TEST_CHECK(!fixture.control->setVerticalPropeller(10U, -101));
        TEST_CHECK(!fixture.control->setHorizontalPropeller(13U, 0));
        TEST_CHECK(!fixture.control->setHorizontalBase(-101));
        TEST_CHECK(!fixture.control->getPropellerBase(9U));
        TEST_CHECK(fixture.transport->commands().empty());
    });

    suite.run("RovControl propeller safety is preserved", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) + " err safety\r\n");
            });
        const auto result = fixture.control->setVerticalPropeller(10U, 1);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == rov::RovError::Safety);
        TEST_CHECK(result.failure.origin == rov::ErrorOrigin::M33);
    });

    suite.run("RovControl propeller base real queries", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                const auto payload = commandPayload(command);
                std::string reply;
                if (payload == "get propeller 10 base") {
                    reply = " ok propeller 10 base -7\r\n";
                } else if (payload == "get propeller 15 real") {
                    reply = " ok propeller 15 real 8\r\n";
                } else if (payload == "get propeller all base") {
                    reply = " ok propeller all base -1 -2 -3 -4 -5 -6\r\n";
                } else {
                    reply = " ok propeller all real 1 2 3 4 5 6\r\n";
                }
                fake.enqueueRead(commandSequence(command) + reply);
            });
        const auto base = fixture.control->getPropellerBase(10U);
        const auto output = fixture.control->getPropellerOutput(15U);
        const auto allBase = fixture.control->getAllPropellerBases();
        const auto allOutput = fixture.control->getAllPropellerOutputs();
        TEST_CHECK(base && *base.value == -7);
        TEST_CHECK(output && *output.value == 8);
        TEST_CHECK(allBase && (*allBase.value)[5] == -6);
        TEST_CHECK(allOutput && (*allOutput.value)[0] == 1);
    });

    suite.run("RovControl stabilization and synchronization modes", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(replyOk);
        TEST_CHECK(fixture.control->enableStabilization());
        checkLastPayload(fixture, "horizontal on");
        TEST_CHECK(fixture.control->disableStabilization());
        checkLastPayload(fixture, "horizontal off");
        TEST_CHECK(fixture.control->enableHorizontalSynchronization());
        checkLastPayload(fixture, "synchronization on");
        TEST_CHECK(fixture.control->disableHorizontalSynchronization());
        checkLastPayload(fixture, "synchronization off");
    });

    suite.run("RovControl group stop move canonical commands", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(replyOk);
        TEST_CHECK(fixture.control->stopVertical());
        checkLastPayload(fixture, "stop vertical");
        TEST_CHECK(fixture.control->moveVertical());
        checkLastPayload(fixture, "move vertical");
        TEST_CHECK(fixture.control->stopHorizontal());
        checkLastPayload(fixture, "stop horizontal");
        TEST_CHECK(fixture.control->moveHorizontal());
        checkLastPayload(fixture, "move horizontal");
    });

    suite.run("RovControl MPU query", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                                 " ok -1 2 -3 4 -5 6\r\n");
            });
        const auto result = fixture.control->readMpu();
        TEST_CHECK(result);
        TEST_CHECK(result.value->ax == -1 && result.value->gz == 6);
        checkLastPayload(fixture, "sensor mpu");
    });

    suite.run("RovControl sensor snapshot query", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                    " ok sensors mpu ready dyp ready state complete busy 0 "
                    "valid 1 distance_mm 65533 age_ms 9\r\n");
            });
        const auto result = fixture.control->getSensorSnapshot();
        TEST_CHECK(result && *result.value->distanceMm == 65533U);
        checkLastPayload(fixture, "sensor all");
    });

    suite.run("RovControl attitude query", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                    " ok attitude roll -250 pitch 125 ready 1\r\n");
            });
        const auto result = fixture.control->getAttitude();
        TEST_CHECK(result && result.value->rollDegrees == -2.5);
        TEST_CHECK(result.value->pitchDegrees == 1.25);
        checkLastPayload(fixture, "get attitude");
    });

    suite.run("RovControl stabilization query", [] {
        auto fixture = makeControl();
        fixture.transport->setCommandHandler(
            [](const std::string& command, FakeTransport& fake) {
                fake.enqueueRead(commandSequence(command) +
                    " ok stabilization re 1 pe -2 rp 3 pp -4 "
                    "ch -1 2 -3 4 ar 1 af 1 he 1 gs 0 vs 0 hs 0\r\n");
            });
        const auto result = fixture.control->getStabilization();
        TEST_CHECK(result && result.value->verticalCorrections[2] == -3);
        TEST_CHECK(result.value->attitudeFresh);
        checkLastPayload(fixture, "get stabilization");
    });
}
