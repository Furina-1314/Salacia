#ifndef ROV_TEST_SUPPORT_HPP
#define ROV_TEST_SUPPORT_HPP

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

class TestSuite final {
public:
    void run(const std::string& name, const std::function<void()>& test)
    {
        ++total_;
        try {
            test();
            ++passed_;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        } catch (...) {
            std::cerr << "[FAIL] " << name << ": unknown exception\n";
        }
    }

    int finish() const
    {
        std::cout << "rov_control_tests: " << passed_ << '/' << total_
                  << " PASS\n";
        return passed_ == total_ ? 0 : 1;
    }

private:
    int total_{0};
    int passed_{0};
};

inline void testCheck(bool condition, const char* expression,
                      const char* file, int line)
{
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" +
            std::to_string(line) + ": check failed: " + expression);
    }
}

#define TEST_CHECK(expression) \
    testCheck(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

void runResponseParserTests(TestSuite& suite);
void runRpmsgClientTests(TestSuite& suite);
void runRovControlTests(TestSuite& suite);
void runExtendedResponseParserTests(TestSuite& suite);
void runExtendedRovControlTests(TestSuite& suite);
void runSelfTestRunnerTests(TestSuite& suite);

#endif // ROV_TEST_SUPPORT_HPP
