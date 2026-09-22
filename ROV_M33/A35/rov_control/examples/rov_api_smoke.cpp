#include "rov/rov_control.hpp"

#include <iomanip>
#include <iostream>
#include <string>

namespace {

template <typename T>
void printFailure(const rov::RovResult<T>& result)
{
    std::cerr << "[FAIL] " << rov::toString(result.failure.code)
              << " origin=" << rov::toString(result.failure.origin);
    if (!result.failure.detail.empty()) {
        std::cerr << " detail=" << result.failure.detail;
    }
    if (!result.failure.rawResponse.empty()) {
        std::cerr << " response='" << result.failure.rawResponse << "'";
    }
    std::cerr << '\n';
}

int runServo(rov::RovControl& control)
{
    const auto result = control.setServo(9U, 30U);
    if (!result) {
        printFailure(result);
        return 1;
    }
    std::cout << "[PASS] setServo(9,30)\n"
              << "       Verify that CH9 moved to approximately 30 degrees.\n";
    return 0;
}

int runDyp(rov::RovControl& control)
{
    const auto result = control.readDyp();
    if (!result) {
        printFailure(result);
        return 1;
    }
    std::cout << "[PASS] DYP response\n"
              << "       distance_mm = " << result.value->distanceMm << '\n'
              << "       A valid UART frame is not necessarily a physically "
                 "valid distance.\n";
    return 0;
}

const char* dypStateName(rov::DypState state)
{
    switch (state) {
    case rov::DypState::Uninitialized: return "uninitialized";
    case rov::DypState::Idle: return "idle";
    case rov::DypState::Waiting: return "waiting";
    case rov::DypState::Complete: return "complete";
    case rov::DypState::Timeout: return "timeout";
    case rov::DypState::IoError: return "io_error";
    }
    return "unknown";
}

int runServoGet(rov::RovControl& control)
{
    const auto result = control.getServo(9U);
    if (!result) {
        printFailure(result);
        return 1;
    }
    std::cout << "[PASS] servo CH9 angle = "
              << static_cast<unsigned int>(*result.value) << '\n';
    return 0;
}

int runMpu(rov::RovControl& control)
{
    const auto result = control.readMpu();
    if (!result) {
        printFailure(result);
        return 1;
    }
    const auto& value = *result.value;
    std::cout << "[PASS] MPU raw ax=" << value.ax << " ay=" << value.ay
              << " az=" << value.az << " gx=" << value.gx
              << " gy=" << value.gy << " gz=" << value.gz << '\n';
    return 0;
}

int runSensors(rov::RovControl& control)
{
    const auto result = control.getSensorSnapshot();
    if (!result) {
        printFailure(result);
        return 1;
    }
    const auto& value = *result.value;
    std::cout << "[PASS] sensors mpu=" << (value.mpuReady ? "ready" : "not_ready")
              << " dyp=" << (value.dypReady ? "ready" : "not_ready")
              << " state=" << dypStateName(value.dypState)
              << " busy=" << value.dypBusy;
    if (value.distanceMm && value.age) {
        std::cout << " distance_mm=" << *value.distanceMm
                  << " age_ms=" << value.age->count();
    } else {
        std::cout << " distance_mm=invalid age_ms=invalid";
    }
    std::cout << '\n';
    return 0;
}

int runAttitude(rov::RovControl& control)
{
    const auto result = control.getAttitude();
    if (!result) {
        printFailure(result);
        return 1;
    }
    std::cout << std::fixed << std::setprecision(2)
              << "[PASS] attitude roll=" << result.value->rollDegrees
              << " pitch=" << result.value->pitchDegrees
              << " ready=" << result.value->ready << '\n';
    return 0;
}

int runStabilization(rov::RovControl& control)
{
    const auto result = control.getStabilization();
    if (!result) {
        printFailure(result);
        return 1;
    }
    const auto& value = *result.value;
    std::cout << std::fixed << std::setprecision(2)
              << "[PASS] stabilization re=" << value.rollErrorDegrees
              << " pe=" << value.pitchErrorDegrees
              << " rp=" << value.rollPidCommand
              << " pp=" << value.pitchPidCommand << " ch=";
    for (const auto correction : value.verticalCorrections) {
        std::cout << correction << ' ';
    }
    std::cout << "ar=" << value.attitudeReady
              << " af=" << value.attitudeFresh
              << " he=" << value.horizontalEnabled
              << " gs=" << value.globalStopped
              << " vs=" << value.verticalStopped
              << " hs=" << value.horizontalStopped << '\n';
    return 0;
}

int runLatch(rov::RovControl& control, bool stop)
{
    const auto result = stop ? control.stop() : control.move();
    if (!result) {
        printFailure(result);
        return 1;
    }
    std::cout << "[PASS] " << (stop ? "stop" : "move") << '\n';
    return 0;
}

void printUsage(const char* program)
{
    std::cerr << "Usage: " << program
              << " <servo|servo-get|dyp|mpu|sensors|attitude|stabilization|"
                 "basic|stop|move> [device]\n"
              << "Warning: while this process owns the RPMsg tty, do not run "
                 "cat or echo against the same device from another process.\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return 2;
    }

    const std::string mode = argv[1];
    const std::string device = argc == 3 ? argv[2] : "/dev/ttyRPMSG0";
    rov::RovControl control(device);

    const auto opened = control.open();
    if (!opened) {
        printFailure(opened);
        return 1;
    }
    std::cout << "[PASS] RPMsg open: " << device << '\n';
    std::cout << "[NOTICE] Do not concurrently cat/echo the same RPMsg tty.\n";

    if (mode == "servo") {
        return runServo(control);
    }
    if (mode == "dyp") {
        return runDyp(control);
    }
    if (mode == "servo-get") {
        return runServoGet(control);
    }
    if (mode == "mpu") {
        return runMpu(control);
    }
    if (mode == "sensors") {
        return runSensors(control);
    }
    if (mode == "attitude") {
        return runAttitude(control);
    }
    if (mode == "stabilization") {
        return runStabilization(control);
    }
    if (mode == "basic") {
        if (runServo(control) != 0) {
            return 1;
        }
        return runDyp(control);
    }
    if (mode == "stop") {
        return runLatch(control, true);
    }
    if (mode == "move") {
        return runLatch(control, false);
    }

    printUsage(argv[0]);
    return 2;
}
