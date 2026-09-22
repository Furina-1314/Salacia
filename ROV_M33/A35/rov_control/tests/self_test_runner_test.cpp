#include "test_support.hpp"

#include "self_test_runner.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

namespace {

rov::RovFailure failure(rov::RovError error,
                        rov::ErrorOrigin origin = rov::ErrorOrigin::M33)
{
    return {error, origin, "injected failure", {}, 0};
}

class FakeSelfTestControl final : public rov::tools::ISelfTestControl {
public:
    FakeSelfTestControl()
    {
        servos.fill(90U);
        outputs.fill(0);
        bases.fill(0);
        snapshot.mpuReady = true;
        snapshot.dypReady = true;
        snapshot.dypState = rov::DypState::Complete;
        snapshot.distanceMm = static_cast<std::uint16_t>(65533U);
        snapshot.age = std::chrono::milliseconds(5);
        attitude.ready = true;
        stabilization.attitudeReady = true;
        stabilization.attitudeFresh = true;
        stabilization.horizontalEnabled = true;
    }

    rov::RovResult<void> open() override
    {
        ++openCalls;
        return openFailure == rov::RovError::None
            ? rov::RovResult<void>::success()
            : rov::RovResult<void>::fail(failure(openFailure,
                                                  rov::ErrorOrigin::Client));
    }

    void close() noexcept override
    {
        ++closeCalls;
    }

    rov::RovResult<std::array<std::uint8_t, 10>> getAllServos() override
    {
        return servoFailure == rov::RovError::None
            ? rov::RovResult<std::array<std::uint8_t, 10>>::success(servos)
            : rov::RovResult<std::array<std::uint8_t, 10>>::fail(
                  failure(servoFailure));
    }

    rov::RovResult<std::array<std::int16_t, 6>>
        getAllPropellerOutputs() override
    {
        return outputFailure == rov::RovError::None
            ? rov::RovResult<std::array<std::int16_t, 6>>::success(outputs)
            : rov::RovResult<std::array<std::int16_t, 6>>::fail(
                  failure(outputFailure));
    }

    rov::RovResult<std::array<std::int16_t, 6>>
        getAllPropellerBases() override
    {
        return baseFailure == rov::RovError::None
            ? rov::RovResult<std::array<std::int16_t, 6>>::success(bases)
            : rov::RovResult<std::array<std::int16_t, 6>>::fail(
                  failure(baseFailure));
    }

    rov::RovResult<rov::MpuRaw> readMpu() override
    {
        return mpuFailure == rov::RovError::None
            ? rov::RovResult<rov::MpuRaw>::success(mpu)
            : rov::RovResult<rov::MpuRaw>::fail(failure(mpuFailure));
    }

    rov::RovResult<rov::DypReading> readDyp() override
    {
        return dypFailure == rov::RovError::None
            ? rov::RovResult<rov::DypReading>::success(
                  {static_cast<std::uint16_t>(65533U)})
            : rov::RovResult<rov::DypReading>::fail(failure(dypFailure));
    }

    rov::RovResult<rov::SensorSnapshot> getSensorSnapshot() override
    {
        return snapshotFailure == rov::RovError::None
            ? rov::RovResult<rov::SensorSnapshot>::success(snapshot)
            : rov::RovResult<rov::SensorSnapshot>::fail(
                  failure(snapshotFailure));
    }

    rov::RovResult<rov::Attitude> getAttitude() override
    {
        return attitudeFailure == rov::RovError::None
            ? rov::RovResult<rov::Attitude>::success(attitude)
            : rov::RovResult<rov::Attitude>::fail(failure(attitudeFailure));
    }

    rov::RovResult<rov::StabilizationStatus> getStabilization() override
    {
        return stabilizationFailure == rov::RovError::None
            ? rov::RovResult<rov::StabilizationStatus>::success(stabilization)
            : rov::RovResult<rov::StabilizationStatus>::fail(
                  failure(stabilizationFailure));
    }

    rov::RovResult<void> stop() override
    {
        ++stopCalls;
        return stopFailure == rov::RovError::None
            ? rov::RovResult<void>::success()
            : rov::RovResult<void>::fail(failure(stopFailure));
    }

    rov::RovResult<std::uint8_t> getServo(std::uint8_t id) override
    {
        if (id != 9U || actuatorQueryFailure != rov::RovError::None) {
            return rov::RovResult<std::uint8_t>::fail(
                failure(actuatorQueryFailure == rov::RovError::None
                    ? rov::RovError::BadArgument : actuatorQueryFailure));
        }
        return rov::RovResult<std::uint8_t>::success(servo9);
    }

    rov::RovResult<void> setServo(std::uint8_t id,
                                  std::uint8_t angle) override
    {
        servoCommands.push_back({id, angle});
        if (actuatorSetFailure != rov::RovError::None) {
            return rov::RovResult<void>::fail(failure(actuatorSetFailure));
        }
        servo9 = angle;
        return rov::RovResult<void>::success();
    }

    rov::RovResult<void> setVerticalBase(std::int16_t value) override
    {
        verticalCommands.push_back(value);
        if (thrusterSetterAccepted) {
            return rov::RovResult<void>::success();
        }
        return rov::RovResult<void>::fail(failure(thrusterSetterFailure));
    }

    rov::RovError openFailure{rov::RovError::None};
    rov::RovError servoFailure{rov::RovError::None};
    rov::RovError outputFailure{rov::RovError::None};
    rov::RovError baseFailure{rov::RovError::None};
    rov::RovError mpuFailure{rov::RovError::None};
    rov::RovError dypFailure{rov::RovError::None};
    rov::RovError snapshotFailure{rov::RovError::None};
    rov::RovError attitudeFailure{rov::RovError::None};
    rov::RovError stabilizationFailure{rov::RovError::None};
    rov::RovError stopFailure{rov::RovError::None};
    rov::RovError actuatorQueryFailure{rov::RovError::None};
    rov::RovError actuatorSetFailure{rov::RovError::None};
    rov::RovError thrusterSetterFailure{rov::RovError::Safety};
    bool thrusterSetterAccepted{false};
    std::array<std::uint8_t, 10> servos{};
    std::array<std::int16_t, 6> outputs{};
    std::array<std::int16_t, 6> bases{};
    rov::MpuRaw mpu{1, 2, 3, 4, 5, 6};
    rov::SensorSnapshot snapshot;
    rov::Attitude attitude{1.0, -2.0, true};
    rov::StabilizationStatus stabilization;
    std::uint8_t servo9{90U};
    int openCalls{0};
    int closeCalls{0};
    int stopCalls{0};
    std::vector<std::pair<std::uint8_t, std::uint8_t>> servoCommands;
    std::vector<std::int16_t> verticalCommands;
};

struct RunResult {
    rov::tools::SelfTestSummary summary;
    std::string output;
};

RunResult run(FakeSelfTestControl& control,
              rov::tools::SelfTestOptions options = {},
              const std::string& inputText = {})
{
    std::istringstream input(inputText);
    std::ostringstream output;
    rov::tools::SelfTestRunner runner(control, input, output);
    return {runner.run(options), output.str()};
}

} // namespace

void runSelfTestRunnerTests(TestSuite& suite)
{
    suite.run("self-test all default checks pass", [] {
        FakeSelfTestControl control;
        const auto result = run(control);
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(result.summary.passed == 10U);
        TEST_CHECK(result.summary.skipped == 2U);
        TEST_CHECK(result.summary.exitCode() == 0);
        TEST_CHECK(control.stopCalls == 2);
        TEST_CHECK(control.closeCalls == 1);
    });

    suite.run("self-test open failure", [] {
        FakeSelfTestControl control;
        control.openFailure = rov::RovError::TransportIo;
        const auto result = run(control);
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(result.summary.exitCode() == 1);
        TEST_CHECK(control.stopCalls == 0);
        TEST_CHECK(control.closeCalls == 1);
    });

    suite.run("self-test DYP timeout is critical", [] {
        FakeSelfTestControl control;
        control.dypFailure = rov::RovError::Timeout;
        const auto result = run(control);
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(result.output.find("error=timeout") != std::string::npos);
    });

    suite.run("self-test MPU failure is critical", [] {
        FakeSelfTestControl control;
        control.mpuFailure = rov::RovError::NotReady;
        const auto result = run(control);
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(result.summary.exitCode() == 1);
    });

    suite.run("self-test sensor snapshot valid", [] {
        FakeSelfTestControl control;
        const auto result = run(control);
        TEST_CHECK(result.output.find("distance_mm=65533 age_ms=5") !=
                   std::string::npos);
        TEST_CHECK(result.summary.failed == 0U);
    });

    suite.run("self-test sensor snapshot invalid is not failure", [] {
        FakeSelfTestControl control;
        control.snapshot.distanceMm.reset();
        control.snapshot.age.reset();
        const auto result = run(control);
        TEST_CHECK(result.output.find("distance_mm=invalid age_ms=invalid") !=
                   std::string::npos);
        TEST_CHECK(result.summary.failed == 0U);
    });

    suite.run("self-test attitude not ready warns", [] {
        FakeSelfTestControl control;
        control.attitude.ready = false;
        const auto result = run(control);
        TEST_CHECK(result.summary.warned == 1U);
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(result.summary.exitCode() == 0);
    });

    suite.run("self-test stabilization stale warns", [] {
        FakeSelfTestControl control;
        control.stabilization.attitudeFresh = false;
        const auto result = run(control);
        TEST_CHECK(result.summary.warned == 1U);
        TEST_CHECK(result.summary.failed == 0U);
    });

    suite.run("self-test base safety skips", [] {
        FakeSelfTestControl control;
        control.baseFailure = rov::RovError::Safety;
        const auto result = run(control);
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(result.summary.skipped == 3U);
        TEST_CHECK(result.output.find("unavailable in current mode: safety") !=
                   std::string::npos);
    });

    suite.run("self-test stop failure fails overall", [] {
        FakeSelfTestControl control;
        control.stopFailure = rov::RovError::Io;
        const auto result = run(control);
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(result.summary.exitCode() == 1);
        TEST_CHECK(control.stopCalls == 2);
        TEST_CHECK(result.output.find("global stop could not be confirmed") !=
                   std::string::npos);
    });

    suite.run("self-test actuator not confirmed skips", [] {
        FakeSelfTestControl control;
        rov::tools::SelfTestOptions options;
        options.actuators = true;
        options.interactive = true;
        const auto result = run(control, options, "NO\n");
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(control.servoCommands.empty());
        TEST_CHECK(result.output.find("confirmation was not YES") !=
                   std::string::npos);
    });

    suite.run("self-test actuator command failure fails", [] {
        FakeSelfTestControl control;
        control.actuatorSetFailure = rov::RovError::Io;
        rov::tools::SelfTestOptions options;
        options.actuators = true;
        options.interactive = true;
        const auto result = run(control, options, "YES\n");
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(result.summary.exitCode() == 1);
    });

    suite.run("self-test actuator command restores CH9", [] {
        FakeSelfTestControl control;
        rov::tools::SelfTestOptions options;
        options.actuators = true;
        options.interactive = true;
        const auto result = run(control, options, "YES\n");
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(control.servoCommands.size() == 2U);
        TEST_CHECK(control.servoCommands[0].second == 95U);
        TEST_CHECK(control.servoCommands[1].second == 90U);
    });

    suite.run("self-test thruster not confirmed skips", [] {
        FakeSelfTestControl control;
        rov::tools::SelfTestOptions options;
        options.thrusters = true;
        options.interactive = true;
        const auto result = run(control, options, "NO\n");
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(control.verticalCommands.empty());
    });

    suite.run("self-test thruster safety sends only zero", [] {
        FakeSelfTestControl control;
        rov::tools::SelfTestOptions options;
        options.thrusters = true;
        options.interactive = true;
        const auto result = run(control, options, "I_UNDERSTAND\n");
        TEST_CHECK(result.summary.failed == 0U);
        TEST_CHECK(control.verticalCommands.size() == 1U);
        TEST_CHECK(control.verticalCommands[0] == 0);
        TEST_CHECK(control.stopCalls == 3);
    });

    suite.run("self-test thruster safety rejects nonzero output state", [] {
        FakeSelfTestControl control;
        control.outputs[2] = 1;
        rov::tools::SelfTestOptions options;
        options.thrusters = true;
        options.interactive = true;
        const auto result = run(control, options, "I_UNDERSTAND\n");
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(result.summary.exitCode() == 1);
    });

    suite.run("self-test cancellation performs cleanup stop", [] {
        FakeSelfTestControl control;
        rov::tools::SelfTestOptions options;
        options.cancellationRequested = [] { return true; };
        const auto result = run(control, options);
        TEST_CHECK(result.summary.failed == 1U);
        TEST_CHECK(control.stopCalls == 1);
        TEST_CHECK(control.closeCalls == 1);
    });

    suite.run("self-test summary count and result text", [] {
        FakeSelfTestControl control;
        control.attitude.ready = false;
        const auto result = run(control);
        TEST_CHECK(result.output.find("SUMMARY: PASS=9 FAIL=0 WARN=1 SKIP=2") !=
                   std::string::npos);
        TEST_CHECK(result.output.find("RESULT: PASS WITH WARNINGS") !=
                   std::string::npos);
    });
}
