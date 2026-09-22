#include "test_support.hpp"

#include "internal/response_parser.hpp"

#include <array>
#include <string>

using rov::ErrorOrigin;
using rov::RovError;
using rov::internal::ExpectedResponseKind;
using rov::internal::ParsedLineKind;
using rov::internal::ResponseParser;

void runResponseParserTests(TestSuite& suite)
{
    suite.run("parser ack", [] {
        const auto line = ResponseParser::parseLine("0001 ok");
        TEST_CHECK(line.lineKind == ParsedLineKind::Response);
        TEST_CHECK(line.hasSequence);
        TEST_CHECK(line.sequence == 1U);
        const auto result = ResponseParser::validate(
            line, ExpectedResponseKind::Ack);
        TEST_CHECK(result);
    });

    suite.run("parser DYP 65533", [] {
        const auto line = ResponseParser::parseLine(
            "0002 ok dyp distance_mm 65533\r");
        const auto result = ResponseParser::validate(
            line, ExpectedResponseKind::Dyp);
        TEST_CHECK(result);
        TEST_CHECK(result.value->dypReading.has_value());
        TEST_CHECK(result.value->dypReading->distanceMm == 65533U);
    });

    const std::array<std::pair<const char*, RovError>, 8> errors{{
        {"bad_cmd", RovError::BadCommand},
        {"bad_arg", RovError::BadArgument},
        {"busy", RovError::Busy},
        {"not_ready", RovError::NotReady},
        {"timeout", RovError::Timeout},
        {"safety", RovError::Safety},
        {"io", RovError::Io},
        {"unsupported", RovError::Unsupported}
    }};
    for (const auto& entry : errors) {
        suite.run(std::string("parser wire error ") + entry.first,
                  [entry] {
            const auto line = ResponseParser::parseLine(
                std::string("0042 err ") + entry.first);
            const auto result = ResponseParser::validate(
                line, ExpectedResponseKind::Ack);
            TEST_CHECK(!result);
            TEST_CHECK(result.failure.code == entry.second);
            TEST_CHECK(result.failure.origin == ErrorOrigin::M33);
        });
    }

    suite.run("parser malformed sequence", [] {
        const auto line = ResponseParser::parseLine("00A1 ok");
        TEST_CHECK(line.lineKind == ParsedLineKind::Malformed);
        TEST_CHECK(!line.hasSequence);
    });

    suite.run("parser unsolicited banner", [] {
        const auto line = ResponseParser::parseLine("ACTUATOR I2C4 READY");
        TEST_CHECK(line.lineKind == ParsedLineKind::Unsolicited);
    });

    suite.run("parser future event", [] {
        const auto line = ResponseParser::parseLine("event ready");
        TEST_CHECK(line.lineKind == ParsedLineKind::Event);
    });

    suite.run("parser malformed DYP missing distance", [] {
        const auto line = ResponseParser::parseLine(
            "0003 ok dyp distance_mm");
        const auto result = ResponseParser::validate(
            line, ExpectedResponseKind::Dyp);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == RovError::ProtocolError);
    });

    suite.run("parser malformed DYP nonnumeric", [] {
        const auto line = ResponseParser::parseLine(
            "0003 ok dyp distance_mm invalid");
        const auto result = ResponseParser::validate(
            line, ExpectedResponseKind::Dyp);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == RovError::ProtocolError);
    });

    suite.run("parser DYP uint16 overflow", [] {
        const auto line = ResponseParser::parseLine(
            "0003 ok dyp distance_mm 65536");
        const auto result = ResponseParser::validate(
            line, ExpectedResponseKind::Dyp);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == RovError::ProtocolError);
    });

    suite.run("parser unexpected success payload", [] {
        const auto line = ResponseParser::parseLine("0004 ok extra");
        const auto result = ResponseParser::validate(
            line, ExpectedResponseKind::Ack);
        TEST_CHECK(!result);
        TEST_CHECK(result.failure.code == RovError::ProtocolError);
    });
}
