#ifndef ROV_TOOLS_SELF_TEST_RUNNER_HPP
#define ROV_TOOLS_SELF_TEST_RUNNER_HPP

#include "rov/rov_control.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>

namespace rov::tools {

enum class SelfTestStatus {
    Pass,
    Fail,
    Warn,
    Skip
};

struct SelfTestOptions {
    bool actuators{false};
    bool thrusters{false};
    bool interactive{false};
    /* When false (module declared absent via gate config), a DYP
     * communication failure degrades to WARN: the gate stays FAIL=0
     * strict for every other check while the sensor is known-missing. */
    bool expectDyp{true};
    std::function<bool()> cancellationRequested;
};

struct SelfTestSummary {
    unsigned int passed{0};
    unsigned int failed{0};
    unsigned int warned{0};
    unsigned int skipped{0};

    int exitCode() const noexcept;
};

class ISelfTestControl {
public:
    virtual ~ISelfTestControl() = default;

    virtual RovResult<void> open() = 0;
    virtual void close() noexcept = 0;
    virtual RovResult<std::array<std::uint8_t, 10>> getAllServos() = 0;
    virtual RovResult<std::array<std::int16_t, 6>>
        getAllPropellerOutputs() = 0;
    virtual RovResult<std::array<std::int16_t, 6>>
        getAllPropellerBases() = 0;
    virtual RovResult<MpuRaw> readMpu() = 0;
    virtual RovResult<DypReading> readDyp() = 0;
    virtual RovResult<SensorSnapshot> getSensorSnapshot() = 0;
    virtual RovResult<Attitude> getAttitude() = 0;
    virtual RovResult<StabilizationStatus> getStabilization() = 0;
    virtual RovResult<void> stop() = 0;
    virtual RovResult<std::uint8_t> getServo(std::uint8_t id) = 0;
    virtual RovResult<void> setServo(std::uint8_t id,
                                     std::uint8_t angle) = 0;
    virtual RovResult<void> setVerticalBase(std::int16_t value) = 0;
};

class SelfTestRunner final {
public:
    SelfTestRunner(ISelfTestControl& control, std::istream& input,
                   std::ostream& output);

    SelfTestSummary run(const SelfTestOptions& options);

private:
    void record(SelfTestStatus status, const char* name,
                const std::string& detail = {});
    bool cancelled(const SelfTestOptions& options);
    void printFailure(const RovFailure& failure);
    void runDefaultChecks(const SelfTestOptions& options);
    void runActuatorCheck(const SelfTestOptions& options);
    void runThrusterCheck(const SelfTestOptions& options);
    void cleanup() noexcept;
    void printSummary() const;

    ISelfTestControl& control_;
    std::istream& input_;
    std::ostream& output_;
    SelfTestSummary summary_;
    bool opened_{false};
    bool interrupted_{false};
    bool safeStopConfirmed_{false};
};

} // namespace rov::tools

#endif // ROV_TOOLS_SELF_TEST_RUNNER_HPP
