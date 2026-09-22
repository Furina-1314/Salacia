#include "self_test_runner.hpp"

#include <algorithm>
#include <iomanip>
#include <exception>
#include <istream>
#include <ostream>
#include <string>

namespace rov::tools {
namespace {

const char* statusName(SelfTestStatus status)
{
    switch (status) {
    case SelfTestStatus::Pass: return "PASS";
    case SelfTestStatus::Fail: return "FAIL";
    case SelfTestStatus::Warn: return "WARN";
    case SelfTestStatus::Skip: return "SKIP";
    }
    return "UNKNOWN";
}

const char* dypStateName(DypState state)
{
    switch (state) {
    case DypState::Uninitialized: return "uninitialized";
    case DypState::Idle: return "idle";
    case DypState::Waiting: return "waiting";
    case DypState::Complete: return "complete";
    case DypState::Timeout: return "timeout";
    case DypState::IoError: return "io_error";
    }
    return "unknown";
}

} // namespace

int SelfTestSummary::exitCode() const noexcept
{
    return failed == 0U ? 0 : 1;
}

SelfTestRunner::SelfTestRunner(ISelfTestControl& control, std::istream& input,
                               std::ostream& output)
    : control_(control), input_(input), output_(output)
{
}

void SelfTestRunner::record(SelfTestStatus status, const char* name,
                            const std::string& detail)
{
    switch (status) {
    case SelfTestStatus::Pass: ++summary_.passed; break;
    case SelfTestStatus::Fail: ++summary_.failed; break;
    case SelfTestStatus::Warn: ++summary_.warned; break;
    case SelfTestStatus::Skip: ++summary_.skipped; break;
    }
    output_ << '[' << statusName(status) << "] " << name << '\n';
    if (!detail.empty()) {
        output_ << "       " << detail << '\n';
    }
}

void SelfTestRunner::printFailure(const RovFailure& failure)
{
    output_ << "       origin=" << toString(failure.origin)
            << " error=" << toString(failure.code) << '\n';
    if (!failure.detail.empty()) {
        output_ << "       detail=" << failure.detail << '\n';
    }
}

bool SelfTestRunner::cancelled(const SelfTestOptions& options)
{
    if (!interrupted_ && options.cancellationRequested &&
        options.cancellationRequested()) {
        interrupted_ = true;
        record(SelfTestStatus::Fail, "Interrupted",
               "cancellation requested; entering safe cleanup");
    }
    return interrupted_;
}

void SelfTestRunner::runDefaultChecks(const SelfTestOptions& options)
{
    const auto servos = control_.getAllServos();
    if (!servos) {
        record(SelfTestStatus::Fail, "Servo state");
        printFailure(servos.failure);
    } else {
        record(SelfTestStatus::Pass, "Servo state");
        for (std::size_t index = 0; index < servos.value->size(); ++index) {
            output_ << "       CH" << index << " = "
                    << static_cast<unsigned int>((*servos.value)[index])
                    << '\n';
        }
    }
    if (cancelled(options)) return;

    const auto outputs = control_.getAllPropellerOutputs();
    if (!outputs) {
        record(SelfTestStatus::Fail, "Propeller output state");
        printFailure(outputs.failure);
    } else {
        record(SelfTestStatus::Pass, "Propeller output state");
        for (std::size_t index = 0; index < outputs.value->size(); ++index) {
            output_ << "       CH" << index + 10U << " = "
                    << (*outputs.value)[index] << '\n';
        }
    }

    const auto bases = control_.getAllPropellerBases();
    if (!bases && bases.failure.code == RovError::Safety &&
        bases.failure.origin == ErrorOrigin::M33) {
        record(SelfTestStatus::Skip, "Propeller base state",
               "unavailable in current mode: safety");
    } else if (!bases) {
        record(SelfTestStatus::Fail, "Propeller base state");
        printFailure(bases.failure);
    } else {
        record(SelfTestStatus::Pass, "Propeller base state");
        for (std::size_t index = 0; index < bases.value->size(); ++index) {
            output_ << "       CH" << index + 10U << " = "
                    << (*bases.value)[index] << '\n';
        }
    }
    if (cancelled(options)) return;

    const auto mpu = control_.readMpu();
    if (!mpu) {
        record(SelfTestStatus::Fail, "MPU6500");
        printFailure(mpu.failure);
    } else {
        record(SelfTestStatus::Pass, "MPU6500");
        output_ << "       ax=" << mpu.value->ax << " ay=" << mpu.value->ay
                << " az=" << mpu.value->az << " gx=" << mpu.value->gx
                << " gy=" << mpu.value->gy << " gz=" << mpu.value->gz
                << '\n';
    }
    if (cancelled(options)) return;

    const auto dyp = control_.readDyp();
    if (!dyp) {
        if (!options.expectDyp) {
            record(SelfTestStatus::Warn, "DYP communication",
                   "module declared absent (--expect-dyp=0)");
            printFailure(dyp.failure);
        } else {
            record(SelfTestStatus::Fail, "DYP communication");
            printFailure(dyp.failure);
        }
    } else {
        record(SelfTestStatus::Pass, "DYP communication");
        output_ << "       distance_mm = " << dyp.value->distanceMm << '\n'
                << "       note: valid UART frame != valid physical distance\n";
    }
    if (cancelled(options)) return;

    const auto sensors = control_.getSensorSnapshot();
    if (!sensors) {
        record(SelfTestStatus::Fail, "Sensor snapshot");
        printFailure(sensors.failure);
    } else {
        record(SelfTestStatus::Pass, "Sensor snapshot");
        const auto& value = *sensors.value;
        output_ << "       mpu=" << (value.mpuReady ? "ready" : "not_ready")
                << " dyp=" << (value.dypReady ? "ready" : "not_ready")
                << " state=" << dypStateName(value.dypState)
                << " busy=" << value.dypBusy << '\n';
        if (value.distanceMm && value.age) {
            output_ << "       distance_mm=" << *value.distanceMm
                    << " age_ms=" << value.age->count() << '\n';
        } else {
            output_ << "       distance_mm=invalid age_ms=invalid\n";
        }
    }
    if (cancelled(options)) return;

    const auto attitude = control_.getAttitude();
    if (!attitude) {
        record(SelfTestStatus::Fail, "Attitude");
        printFailure(attitude.failure);
    } else {
        record(attitude.value->ready ? SelfTestStatus::Pass
                                     : SelfTestStatus::Warn,
               "Attitude", attitude.value->ready ? std::string{}
                   : "data parsed, but startup calibration is not ready");
        output_ << std::fixed << std::setprecision(2)
                << "       roll=" << attitude.value->rollDegrees
                << " pitch=" << attitude.value->pitchDegrees
                << " ready=" << attitude.value->ready << '\n';
    }
    if (cancelled(options)) return;

    const auto stabilization = control_.getStabilization();
    if (!stabilization) {
        record(SelfTestStatus::Fail, "Stabilization");
        printFailure(stabilization.failure);
    } else {
        const auto& value = *stabilization.value;
        const bool ready = value.attitudeReady && value.attitudeFresh;
        record(ready ? SelfTestStatus::Pass : SelfTestStatus::Warn,
               "Stabilization", ready ? std::string{}
                   : "telemetry parsed, but attitude is not ready/fresh");
        output_ << std::fixed << std::setprecision(2)
                << "       rollError=" << value.rollErrorDegrees
                << " pitchError=" << value.pitchErrorDegrees
                << " rollPid=" << value.rollPidCommand
                << " pitchPid=" << value.pitchPidCommand << '\n'
                << "       CH10=" << value.verticalCorrections[0]
                << " CH11=" << value.verticalCorrections[1]
                << " CH12=" << value.verticalCorrections[2]
                << " CH13=" << value.verticalCorrections[3] << '\n'
                << "       attitudeReady=" << value.attitudeReady
                << " attitudeFresh=" << value.attitudeFresh
                << " stabilizationEnabled=" << value.horizontalEnabled
                << " globalStopped=" << value.globalStopped
                << " verticalStopped=" << value.verticalStopped
                << " horizontalStopped=" << value.horizontalStopped << '\n';
    }
    if (cancelled(options)) return;

    const auto stopped = control_.stop();
    if (!stopped) {
        record(SelfTestStatus::Fail, "Safe stop");
        printFailure(stopped.failure);
    } else {
        safeStopConfirmed_ = true;
        record(SelfTestStatus::Pass, "Safe stop",
               "global stop remains latched after self-test");
    }
}

void SelfTestRunner::runActuatorCheck(const SelfTestOptions& options)
{
    if (!options.actuators) {
        record(SelfTestStatus::Skip, "Servo motion test",
               "use --actuators");
        return;
    }
    if (!options.interactive) {
        record(SelfTestStatus::Skip, "Servo motion test",
               "interactive TTY confirmation is required");
        return;
    }

    output_ << "WARNING:\n"
            << "Servo motion test will move CH9. Ensure mechanisms are clear.\n"
            << "Type YES to continue: " << std::flush;
    std::string confirmation;
    std::getline(input_, confirmation);
    if (confirmation != "YES") {
        record(SelfTestStatus::Skip, "Servo motion test",
               "confirmation was not YES");
        return;
    }

    const auto original = control_.getServo(9U);
    if (!original) {
        record(SelfTestStatus::Fail, "Servo command path");
        printFailure(original.failure);
        return;
    }
    const std::uint8_t target = *original.value <= 90U
        ? static_cast<std::uint8_t>(*original.value + 5U)
        : static_cast<std::uint8_t>(*original.value - 5U);
    const auto moved = control_.setServo(9U, target);
    if (!moved) {
        record(SelfTestStatus::Fail, "Servo command path");
        printFailure(moved.failure);
        return;
    }
    const auto observed = control_.getServo(9U);
    const auto restored = control_.setServo(9U, *original.value);
    if (!observed || *observed.value != target || !restored) {
        record(SelfTestStatus::Fail, "Servo command path",
               "command/query/restore sequence failed");
        if (!observed) printFailure(observed.failure);
        if (!restored) printFailure(restored.failure);
        return;
    }
    record(SelfTestStatus::Pass, "Servo command path",
           "CH9 command accepted and restored; visually confirm motion");
}

void SelfTestRunner::runThrusterCheck(const SelfTestOptions& options)
{
    if (!options.thrusters) {
        record(SelfTestStatus::Skip, "Thruster safety test",
               "use --thrusters");
        return;
    }
    if (!options.interactive) {
        record(SelfTestStatus::Skip, "Thruster safety test",
               "interactive TTY confirmation is required");
        return;
    }

    output_ << "WARNING: THRUSTER TEST MODE.\n"
            << "This first version sends no non-zero thruster command.\n"
            << "Real thruster behavior remains hardware-dependent.\n"
            << "Type I_UNDERSTAND to continue: " << std::flush;
    std::string confirmation;
    std::getline(input_, confirmation);
    if (confirmation != "I_UNDERSTAND") {
        record(SelfTestStatus::Skip, "Thruster safety test",
               "dangerous-mode confirmation was not provided");
        return;
    }

    const auto stopped = control_.stop();
    if (!stopped) {
        record(SelfTestStatus::Fail, "Thruster safety test",
               "could not establish global stop before safety check");
        printFailure(stopped.failure);
        return;
    }
    safeStopConfirmed_ = true;
    const auto setter = control_.setVerticalBase(0);
    const auto outputs = control_.getAllPropellerOutputs();
    const bool outputsAreZero = outputs && std::all_of(
        outputs.value->begin(), outputs.value->end(),
        [](std::int16_t value) { return value == 0; });
    if (setter || setter.failure.code != RovError::Safety ||
        setter.failure.origin != ErrorOrigin::M33 || !outputsAreZero) {
        record(SelfTestStatus::Fail, "Thruster safety test",
               "global stop did not produce the expected safety response/state");
        if (!setter && setter.failure.code != RovError::Safety) {
            printFailure(setter.failure);
        }
        if (!outputs) printFailure(outputs.failure);
        return;
    }
    record(SelfTestStatus::Pass, "Thruster safety test",
           "zero setter rejected by global stop; no non-zero command issued");
}

void SelfTestRunner::cleanup() noexcept
{
    if (!opened_) return;
    try {
        if (control_.stop()) {
            safeStopConfirmed_ = true;
        }
    } catch (...) {
    }
    control_.close();
    opened_ = false;
}

void SelfTestRunner::printSummary() const
{
    output_ << "========================================\n"
            << "SUMMARY: PASS=" << summary_.passed
            << " FAIL=" << summary_.failed
            << " WARN=" << summary_.warned
            << " SKIP=" << summary_.skipped << '\n';
    if (summary_.failed != 0U) {
        output_ << "RESULT: FAIL\n";
    } else if (summary_.warned != 0U || summary_.skipped != 0U) {
        output_ << "RESULT: PASS WITH WARNINGS\n";
    } else {
        output_ << "RESULT: PASS\n";
    }
    if (safeStopConfirmed_) {
        output_ << "The global stop remains latched. Call move() explicitly before motion.\n";
    } else {
        output_ << "WARNING: global stop could not be confirmed. Keep the vehicle safe.\n";
    }
}

SelfTestSummary SelfTestRunner::run(const SelfTestOptions& options)
{
    summary_ = {};
    opened_ = false;
    interrupted_ = false;
    safeStopConfirmed_ = false;
    output_ << "ROV SELF TEST\n"
            << "========================================\n"
            << "Do not concurrently cat/echo /dev/ttyRPMSG0.\n";

    try {
        const auto opened = control_.open();
        if (!opened) {
            record(SelfTestStatus::Fail, "RPMsg open");
            printFailure(opened.failure);
            control_.close();
            printSummary();
            return summary_;
        }
        opened_ = true;
        record(SelfTestStatus::Pass, "RPMsg open");

        if (!cancelled(options)) runDefaultChecks(options);
        if (!cancelled(options)) runActuatorCheck(options);
        if (!cancelled(options)) runThrusterCheck(options);
    } catch (const std::exception& error) {
        record(SelfTestStatus::Fail, "Unhandled self-test exception",
               error.what());
    } catch (...) {
        record(SelfTestStatus::Fail, "Unhandled self-test exception",
               "unknown exception");
    }

    cleanup();
    printSummary();
    return summary_;
}

} // namespace rov::tools
