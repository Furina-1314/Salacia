#include "test_support.hpp"

int main()
{
    TestSuite suite;
    runResponseParserTests(suite);
    runExtendedResponseParserTests(suite);
    runRpmsgClientTests(suite);
    runRovControlTests(suite);
    runExtendedRovControlTests(suite);
    runSelfTestRunnerTests(suite);
    return suite.finish();
}
