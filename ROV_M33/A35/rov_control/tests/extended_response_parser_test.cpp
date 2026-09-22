#include "test_support.hpp"

#include "internal/response_parser.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <string>

using rov::DypState;
using rov::RovError;
using rov::internal::ExpectedResponseKind;
using rov::internal::ResponseParser;

namespace {

rov::RovResult<rov::internal::ClientResponse> validate(
    const std::string& line, ExpectedResponseKind expected)
{
    return ResponseParser::validate(ResponseParser::parseLine(line), expected);
}

void checkProtocolError(
    const std::string& line, ExpectedResponseKind expected)
{
    const auto result = validate(line, expected);
    TEST_CHECK(!result);
    TEST_CHECK(result.failure.code == RovError::ProtocolError);
}

} // namespace

void runExtendedResponseParserTests(TestSuite& suite)
{
    suite.run("parser servo one", [] {
        const auto result = validate("0010 ok servo 3 180",
                                     ExpectedResponseKind::Servo);
        TEST_CHECK(result);
        TEST_CHECK(*result.value->servoId == 3U);
        TEST_CHECK(*result.value->servoAngle == 180U);
    });

    suite.run("parser servo all ten angles", [] {
        const auto result = validate(
            "0011 ok servo all 0 10 20 30 40 50 60 70 80 180",
            ExpectedResponseKind::AllServos);
        TEST_CHECK(result);
        TEST_CHECK(result.value->servoAngles->front() == 0U);
        TEST_CHECK(result.value->servoAngles->back() == 180U);
    });

    suite.run("parser servo malformed count and range", [] {
        checkProtocolError("0012 ok servo all 0 1",
                           ExpectedResponseKind::AllServos);
        checkProtocolError("0012 ok servo 3 181",
                           ExpectedResponseKind::Servo);
    });

    suite.run("parser propeller single base and real", [] {
        const auto base = validate("0020 ok propeller 10 base -100",
                                   ExpectedResponseKind::PropellerBase);
        const auto real = validate("0021 ok propeller 15 real 100",
                                   ExpectedResponseKind::PropellerOutput);
        TEST_CHECK(base && real);
        TEST_CHECK(*base.value->propellerValue == -100);
        TEST_CHECK(*real.value->propellerValue == 100);
    });

    suite.run("parser propeller all six values", [] {
        const auto result = validate(
            "0022 ok propeller all real -100 -2 0 4 99 100",
            ExpectedResponseKind::AllPropellerOutputs);
        TEST_CHECK(result);
        TEST_CHECK((*result.value->propellerValues)[0] == -100);
        TEST_CHECK((*result.value->propellerValues)[5] == 100);
    });

    suite.run("parser propeller wrong field and count", [] {
        checkProtocolError("0023 ok propeller 10 real 1",
                           ExpectedResponseKind::PropellerBase);
        checkProtocolError("0023 ok propeller all base 1 2 3 4 5",
                           ExpectedResponseKind::AllPropellerBases);
        checkProtocolError("0023 ok propeller 10 base 101",
                           ExpectedResponseKind::PropellerBase);
    });

    suite.run("parser MPU signed values", [] {
        const auto result = validate(
            "0030 ok -32768 -123 0 456 789 32767",
            ExpectedResponseKind::Mpu);
        TEST_CHECK(result);
        TEST_CHECK(result.value->mpu->ax == -32768);
        TEST_CHECK(result.value->mpu->gy == 789);
        TEST_CHECK(result.value->mpu->gz == 32767);
    });

    suite.run("parser MPU malformed count and overflow", [] {
        checkProtocolError("0031 ok 1 2 3 4 5",
                           ExpectedResponseKind::Mpu);
        checkProtocolError("0031 ok 1 2 3 4 5 32768",
                           ExpectedResponseKind::Mpu);
    });

    suite.run("parser sensor snapshot valid cache", [] {
        const auto result = validate(
            "0040 ok sensors mpu ready dyp ready state complete busy 0 "
            "valid 1 distance_mm 65533 age_ms 123",
            ExpectedResponseKind::SensorSnapshot);
        TEST_CHECK(result);
        const auto& snapshot = *result.value->sensorSnapshot;
        TEST_CHECK(snapshot.mpuReady && snapshot.dypReady);
        TEST_CHECK(snapshot.dypState == DypState::Complete);
        TEST_CHECK(!snapshot.dypBusy);
        TEST_CHECK(*snapshot.distanceMm == 65533U);
        TEST_CHECK(*snapshot.age == std::chrono::milliseconds(123));
    });

    suite.run("parser sensor snapshot invalid cache", [] {
        const auto result = validate(
            "0041 ok sensors mpu not_ready dyp ready state idle busy 0 "
            "valid 0 distance_mm invalid age_ms invalid",
            ExpectedResponseKind::SensorSnapshot);
        TEST_CHECK(result);
        TEST_CHECK(!result.value->sensorSnapshot->mpuReady);
        TEST_CHECK(!result.value->sensorSnapshot->distanceMm);
        TEST_CHECK(!result.value->sensorSnapshot->age);
    });

    suite.run("parser sensor snapshot all DYP states", [] {
        const std::array<std::pair<const char*, DypState>, 6> states{{
            {"uninitialized", DypState::Uninitialized},
            {"idle", DypState::Idle},
            {"waiting", DypState::Waiting},
            {"complete", DypState::Complete},
            {"timeout", DypState::Timeout},
            {"io_error", DypState::IoError}
        }};
        for (const auto& state : states) {
            const auto result = validate(
                std::string("0042 ok sensors mpu ready dyp ready state ") +
                state.first + " busy 0 valid 0 distance_mm invalid age_ms invalid",
                ExpectedResponseKind::SensorSnapshot);
            TEST_CHECK(result);
            TEST_CHECK(result.value->sensorSnapshot->dypState == state.second);
        }
    });

    suite.run("parser sensor snapshot malformed state and bool", [] {
        checkProtocolError(
            "0043 ok sensors mpu ready dyp ready state bad busy 0 valid 0 "
            "distance_mm invalid age_ms invalid",
            ExpectedResponseKind::SensorSnapshot);
        checkProtocolError(
            "0043 ok sensors mpu ready dyp ready state idle busy 2 valid 0 "
            "distance_mm invalid age_ms invalid",
            ExpectedResponseKind::SensorSnapshot);
    });

    suite.run("parser sensor snapshot age negative and overflow", [] {
        checkProtocolError(
            "0044 ok sensors mpu ready dyp ready state complete busy 0 valid 1 "
            "distance_mm 1 age_ms -1",
            ExpectedResponseKind::SensorSnapshot);
        checkProtocolError(
            "0044 ok sensors mpu ready dyp ready state complete busy 0 valid 1 "
            "distance_mm 1 age_ms 9223372036854775808",
            ExpectedResponseKind::SensorSnapshot);
    });

    suite.run("parser attitude signed centidegrees", [] {
        const auto result = validate(
            "0050 ok attitude roll -1234 pitch 567 ready 1",
            ExpectedResponseKind::Attitude);
        TEST_CHECK(result);
        TEST_CHECK(std::abs(result.value->attitude->rollDegrees + 12.34) < 0.0001);
        TEST_CHECK(std::abs(result.value->attitude->pitchDegrees - 5.67) < 0.0001);
        TEST_CHECK(result.value->attitude->ready);
    });

    suite.run("parser attitude malformed", [] {
        checkProtocolError("0051 ok attitude roll 1 yaw 2 ready 1",
                           ExpectedResponseKind::Attitude);
        checkProtocolError("0051 ok attitude roll 1 pitch 2 ready true",
                           ExpectedResponseKind::Attitude);
    });

    suite.run("parser stabilization complete payload", [] {
        const auto result = validate(
            "0060 ok stabilization re -125 pe 250 rp -375 pp 400 "
            "ch -10 2 3 10 ar 1 af 0 he 1 gs 0 vs 1 hs 0",
            ExpectedResponseKind::Stabilization);
        TEST_CHECK(result);
        const auto& status = *result.value->stabilization;
        TEST_CHECK(std::abs(status.rollErrorDegrees + 1.25) < 0.0001);
        TEST_CHECK(std::abs(status.pitchPidCommand - 4.0) < 0.0001);
        TEST_CHECK(status.verticalCorrections[0] == -10);
        TEST_CHECK(status.verticalCorrections[3] == 10);
        TEST_CHECK(status.attitudeReady && !status.attitudeFresh);
        TEST_CHECK(status.horizontalEnabled && status.verticalStopped);
    });

    suite.run("parser stabilization missing token", [] {
        checkProtocolError(
            "0061 ok stabilization re 1 pe 2 rp 3 pp 4 ch 1 2 3 4 "
            "ar 1 af 1 he 1 gs 0 vs 0",
            ExpectedResponseKind::Stabilization);
    });

    suite.run("parser stabilization wrong label and bool", [] {
        checkProtocolError(
            "0062 ok stabilization re 1 pe 2 rp 3 pp 4 xx 1 2 3 4 "
            "ar 1 af 1 he 1 gs 0 vs 0 hs 0",
            ExpectedResponseKind::Stabilization);
        checkProtocolError(
            "0062 ok stabilization re 1 pe 2 rp 3 pp 4 ch 1 2 3 4 "
            "ar 1 af 1 he 2 gs 0 vs 0 hs 0",
            ExpectedResponseKind::Stabilization);
    });
}
