#include "self_test_runner.hpp"

#include "rov/rov_control.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

std::atomic<bool> cancellationRequested{false};

extern "C" void handleSignal(int)
{
    cancellationRequested.store(true, std::memory_order_relaxed);
}

bool stdinIsInteractive()
{
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

void printUsage(const char* program)
{
    std::cout
        << "Usage: " << program
        << " [--actuators] [--thrusters] [--all] [--expect-dyp=0|1]"
        << " [--device PATH] [--help]\n"
        << "\n"
        << "  default       Safe query checks; finishes with global stop latched\n"
        << "  --actuators   Add confirmed, small CH9 servo command/restore test\n"
        << "  --thrusters   Add confirmed stop-latch safety test; no non-zero command\n"
        << "  --all         Enable both optional modes\n"
        << "  --expect-dyp  1 (default): DYP comm failure is FAIL. 0: module is\n"
        << "                declared absent; a DYP comm failure degrades to WARN\n"
        << "                so the startup gate can pass without the sensor\n"
        << "\n"
        << "Do not concurrently cat or echo the selected RPMsg tty.\n";
}

class RovControlAdapter final : public rov::tools::ISelfTestControl {
public:
    explicit RovControlAdapter(std::string device)
        : control_(std::move(device))
    {
    }

    rov::RovResult<void> open() override { return control_.open(); }
    void close() noexcept override { control_.close(); }
    rov::RovResult<std::array<std::uint8_t, 10>> getAllServos() override
    {
        return control_.getAllServos();
    }
    rov::RovResult<std::array<std::int16_t, 6>>
        getAllPropellerOutputs() override
    {
        return control_.getAllPropellerOutputs();
    }
    rov::RovResult<std::array<std::int16_t, 6>>
        getAllPropellerBases() override
    {
        return control_.getAllPropellerBases();
    }
    rov::RovResult<rov::MpuRaw> readMpu() override
    {
        return control_.readMpu();
    }
    rov::RovResult<rov::DypReading> readDyp() override
    {
        return control_.readDyp();
    }
    rov::RovResult<rov::SensorSnapshot> getSensorSnapshot() override
    {
        return control_.getSensorSnapshot();
    }
    rov::RovResult<rov::Attitude> getAttitude() override
    {
        return control_.getAttitude();
    }
    rov::RovResult<rov::StabilizationStatus> getStabilization() override
    {
        return control_.getStabilization();
    }
    rov::RovResult<void> stop() override { return control_.stop(); }
    rov::RovResult<std::uint8_t> getServo(std::uint8_t id) override
    {
        return control_.getServo(id);
    }
    rov::RovResult<void> setServo(std::uint8_t id,
                                  std::uint8_t angle) override
    {
        return control_.setServo(id, angle);
    }
    rov::RovResult<void> setVerticalBase(std::int16_t value) override
    {
        return control_.setVerticalBase(value);
    }

private:
    rov::RovControl control_;
};

} // namespace

int main(int argc, char** argv)
{
    rov::tools::SelfTestOptions options;
    std::string device = "/dev/ttyRPMSG0";

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (argument == "--actuators") {
            options.actuators = true;
        } else if (argument == "--thrusters") {
            options.thrusters = true;
        } else if (argument == "--all") {
            options.actuators = true;
            options.thrusters = true;
        } else if (argument == "--expect-dyp=0") {
            options.expectDyp = false;
        } else if (argument == "--expect-dyp=1") {
            options.expectDyp = true;
        } else if (argument == "--device" && index + 1 < argc) {
            device = argv[++index];
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            printUsage(argv[0]);
            return 2;
        }
    }

    options.interactive = stdinIsInteractive();
    options.cancellationRequested = [] {
        return cancellationRequested.load(std::memory_order_relaxed);
    };
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    RovControlAdapter control(std::move(device));
    rov::tools::SelfTestRunner runner(control, std::cin, std::cout);
    return runner.run(options).exitCode();
}
