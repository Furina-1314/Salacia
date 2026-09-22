#include "command_service.h"
#include "actuator_service.h"
#include "attitude_estimator.h"
#include "propeller_service.h"
#include "sensor_service.h"
#include "stability_control.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static char reply[160];
static uint8_t last_servo_id;
static uint8_t last_servo_angle;
static uint8_t last_propeller_id;
static int16_t last_propeller_value;
static PropellerService_StatusTypeDef next_propeller_status;
static uint32_t reported_hal_error;
static uint32_t reported_hal_error_a;
static uint32_t reported_hal_error_b;
static unsigned int last_stop_action;
static SensorService_StatusTypeDef next_dyp_start_status;
static SensorService_DypSnapshotTypeDef dyp_snapshot;
static unsigned int dyp_start_count;
static uint8_t mpu_ready = 1U;
static uint32_t dyp_hal_error = 0xD9000055U;

static void capture_reply(const char *message)
{
    (void)strncpy(reply, message, sizeof(reply) - 1U);
    reply[sizeof(reply) - 1U] = '\0';
}

static void capture_io_error(uint32_t hal_error)
{
    reported_hal_error = hal_error;
}

static void capture_io_error_a(uint32_t hal_error)
{
    reported_hal_error_a = hal_error;
}

static void capture_io_error_b(uint32_t hal_error)
{
    reported_hal_error_b = hal_error;
}

uint32_t ActuatorService_GetLastHalError(void)
{
    return 0x1234U;
}

ActuatorService_StatusTypeDef ActuatorService_SetServo(uint8_t id,
                                                       uint8_t angle)
{
    last_servo_id = id;
    last_servo_angle = angle;
    return ACTUATOR_SERVICE_STATUS_OK;
}

ActuatorService_StatusTypeDef ActuatorService_SetServoAll(uint8_t angle)
{
    last_servo_angle = angle;
    return ACTUATOR_SERVICE_STATUS_OK;
}

ActuatorService_StatusTypeDef ActuatorService_GetServo(uint8_t id,
                                                       uint8_t *angle)
{
    last_servo_id = id;
    *angle = 90U;
    return ACTUATOR_SERVICE_STATUS_OK;
}

ActuatorService_StatusTypeDef ActuatorService_GetServoAll(uint8_t *angles)
{
    memset(angles, 90, ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT);
    return ACTUATOR_SERVICE_STATUS_OK;
}

ActuatorService_StatusTypeDef ActuatorService_SetServoMid(uint8_t id)
{
    last_servo_id = id;
    return ACTUATOR_SERVICE_STATUS_OK;
}

ActuatorService_StatusTypeDef ActuatorService_SetServoAllMid(void)
{
    return ACTUATOR_SERVICE_STATUS_OK;
}

static PropellerService_StatusTypeDef propeller_result(void)
{
    PropellerService_StatusTypeDef result = next_propeller_status;
    next_propeller_status = PROPELLER_SERVICE_STATUS_OK;
    return result;
}

PropellerService_StatusTypeDef PropellerService_SetHorizontalEnabled(uint8_t enabled)
{
    last_propeller_value = (int16_t)enabled;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_SetSynchronizationEnabled(
    uint8_t enabled)
{
    last_propeller_value = (int16_t)enabled;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_SetVerticalBase(int16_t command)
{
    last_propeller_value = command;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_SetVertical(uint8_t channel,
                                                            int16_t command)
{
    last_propeller_id = channel;
    last_propeller_value = command;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_SetHorizontalBase(int16_t command)
{
    last_propeller_value = command;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_SetHorizontal(uint8_t channel,
                                                              int16_t command)
{
    last_propeller_id = channel;
    last_propeller_value = command;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_StopAll(void)
{
    last_stop_action = 1U;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_MoveAll(void)
{
    last_stop_action = 2U;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_StopVertical(void)
{
    last_stop_action = 3U;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_MoveVertical(void)
{
    last_stop_action = 4U;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_StopHorizontal(void)
{
    last_stop_action = 5U;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_MoveHorizontal(void)
{
    last_stop_action = 6U;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_GetBase(uint8_t channel,
                                                        int16_t *command)
{
    last_propeller_id = channel;
    *command = 25;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_GetAllBase(int16_t *commands)
{
    memset(commands, 0, PROPELLER_SERVICE_CHANNEL_COUNT * sizeof(commands[0]));
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_GetReal(uint8_t channel,
                                                        int16_t *command)
{
    last_propeller_id = channel;
    *command = -30;
    return propeller_result();
}

PropellerService_StatusTypeDef PropellerService_GetAllReal(int16_t *commands)
{
    memset(commands, 0, PROPELLER_SERVICE_CHANNEL_COUNT * sizeof(commands[0]));
    return propeller_result();
}

SensorService_StatusTypeDef SensorService_ReadMpuRaw(MPU6500_RawData *data)
{
    memset(data, 0, sizeof(*data));
    return SENSOR_SERVICE_STATUS_OK;
}

uint8_t SensorService_IsMpuReady(void)
{
    return mpu_ready;
}

SensorService_StatusTypeDef SensorService_StartDypMeasurement(void)
{
    ++dyp_start_count;
    return next_dyp_start_status;
}

SensorService_StatusTypeDef SensorService_GetDypSnapshot(
    SensorService_DypSnapshotTypeDef *snapshot)
{
    *snapshot = dyp_snapshot;
    return SENSOR_SERVICE_STATUS_OK;
}

uint32_t SensorService_GetDypLastHalError(void)
{
    return dyp_hal_error;
}

AttitudeEstimator_StatusTypeDef AttitudeEstimator_GetState(
    AttitudeEstimator_StateTypeDef *state)
{
    memset(state, 0, sizeof(*state));
    state->roll_deg = 1.25f;
    state->pitch_deg = -2.5f;
    state->ready = 1U;
    return ATTITUDE_ESTIMATOR_STATUS_OK;
}

StabilityControl_StatusTypeDef StabilityControl_GetTelemetry(
    StabilityControl_TelemetryTypeDef *telemetry)
{
    memset(telemetry, 0, sizeof(*telemetry));
    telemetry->roll_error_deg = -1.25f;
    telemetry->pitch_error_deg = 2.5f;
    telemetry->vertical_commands[0] = 10;
    telemetry->vertical_commands[1] = 11;
    telemetry->vertical_commands[2] = 12;
    telemetry->vertical_commands[3] = 13;
    telemetry->attitude_ready = 1U;
    telemetry->attitude_fresh = 1U;
    telemetry->horizontal_enabled = 1U;
    return STABILITY_CONTROL_STATUS_OK;
}

static void test_unified_sequence(void)
{
    CommandService_Handle("0001 set servo 3 90", capture_reply, capture_io_error);
    assert(strcmp(reply, "0001 ok\r\n") == 0);
    assert(last_servo_id == 3U);
    assert(last_servo_angle == 90U);

    CommandService_Handle("0002 set propeller vertical 10 40", capture_reply, capture_io_error);
    assert(strcmp(reply, "0002 ok\r\n") == 0);
    assert(last_propeller_id == 10U);
    assert(last_propeller_value == 40);

    CommandService_Handle("0003\tset servo 4 80", capture_reply, capture_io_error);
    assert(strcmp(reply, "0003 ok\r\n") == 0);
    CommandService_Handle("0004\tset propeller vertical 11 -20", capture_reply, capture_io_error);
    assert(strcmp(reply, "0004 ok\r\n") == 0);
}

static void test_errors_and_removed_commands(void)
{
    next_propeller_status = PROPELLER_SERVICE_STATUS_SAFETY;
    CommandService_Handle("0005 set propeller vertical 10 20", capture_reply, capture_io_error);
    assert(strcmp(reply, "0005 err safety\r\n") == 0);

    CommandService_Handle("0006 set propeller vertical 9 40", capture_reply, capture_io_error);
    assert(strcmp(reply, "0006 err bad_arg\r\n") == 0);
    CommandService_Handle("0007 set propeller vertical 10 101", capture_reply, capture_io_error);
    assert(strcmp(reply, "0007 err bad_arg\r\n") == 0);
    CommandService_Handle("0008 set propeller all 20", capture_reply, capture_io_error);
    assert(strcmp(reply, "0008 err bad_arg\r\n") == 0);
    CommandService_Handle("0009 vertical 10 40", capture_reply, capture_io_error);
    assert(strcmp(reply, "0009 err bad_cmd\r\n") == 0);

    /* Non-four-digit prefixes are not envelopes for either command family. */
    CommandService_Handle("010 set servo 3 90", capture_reply, capture_io_error);
    assert(strcmp(reply, "err bad_cmd\r\n") == 0);
    CommandService_Handle("010 set propeller vertical 10 40", capture_reply, capture_io_error);
    assert(strcmp(reply, "err bad_cmd\r\n") == 0);

    reported_hal_error = 0U;
    reported_hal_error_a = 0U;
    reported_hal_error_b = 0U;
    next_propeller_status = PROPELLER_SERVICE_STATUS_IO_ERROR;
    CommandService_Handle("0013 set propeller vertical 10 20",
                          capture_reply, capture_io_error);
    assert(strcmp(reply, "0013 err io\r\n") == 0);
    assert(reported_hal_error == 0x1234U);
}

static void test_stop_and_queries(void)
{
    CommandService_Handle("0010 stop", capture_reply, capture_io_error);
    assert(strcmp(reply, "0010 ok\r\n") == 0);
    assert(last_stop_action == 1U);
    CommandService_Handle("0011 move", capture_reply, capture_io_error);
    assert(last_stop_action == 2U);
    CommandService_Handle("0012 stop vertical", capture_reply, capture_io_error);
    assert(last_stop_action == 3U);
    CommandService_Handle("0013 move vertical", capture_reply, capture_io_error);
    assert(last_stop_action == 4U);
    CommandService_Handle("0014 stop horizontal", capture_reply, capture_io_error);
    assert(last_stop_action == 5U);
    CommandService_Handle("0015 move horizontal", capture_reply, capture_io_error);
    assert(last_stop_action == 6U);
    CommandService_Handle("0016 set propeller all stop", capture_reply,
                          capture_io_error);
    assert(strcmp(reply, "0016 err bad_arg\r\n") == 0);

    CommandService_Handle("0011 get propeller 10 base", capture_reply, capture_io_error);
    assert(strcmp(reply, "0011 ok propeller 10 base 25\r\n") == 0);
    CommandService_Handle("0012 get propeller 14 real", capture_reply, capture_io_error);
    assert(strcmp(reply, "0012 ok propeller 14 real -30\r\n") == 0);

    CommandService_Handle("0017 get attitude", capture_reply, capture_io_error);
    assert(strcmp(reply, "0017 ok attitude roll 125 pitch -250 ready 1\r\n") == 0);
    CommandService_Handle("0018 get stabilization", capture_reply,
                          capture_io_error);
    assert(strstr(reply, "0018 ok stabilization re -125 pe 250") == reply);
    assert(strstr(reply, "ch 10 11 12 13 ar 1 af 1 he 1") != NULL);
}

static void reset_dyp_command_state(void)
{
    CommandService_Init();
    next_dyp_start_status = SENSOR_SERVICE_STATUS_OK;
    memset(&dyp_snapshot, 0, sizeof(dyp_snapshot));
    dyp_snapshot.state = SENSOR_SERVICE_DYP_IDLE;
    dyp_snapshot.ready = 1U;
    dyp_start_count = 0U;
    mpu_ready = 1U;
    reported_hal_error = 0U;
    (void)strcpy(reply, "unchanged");
}

static void test_dyp_pending_sequence(void)
{
    reset_dyp_command_state();
    CommandService_Handle("0008 sensor dyp", capture_reply, capture_io_error);
    assert(dyp_start_count == 1U);
    assert(strcmp(reply, "unchanged") == 0);

    dyp_snapshot.state = SENSOR_SERVICE_DYP_WAITING;
    dyp_snapshot.busy = 1U;
    CommandService_Handle("0009 sensor dyp", capture_reply, capture_io_error);
    assert(strcmp(reply, "0009 err busy\r\n") == 0);
    assert(dyp_start_count == 1U);

    dyp_snapshot.state = SENSOR_SERVICE_DYP_COMPLETE;
    dyp_snapshot.busy = 0U;
    dyp_snapshot.valid = 1U;
    dyp_snapshot.distance_mm = 742U;
    CommandService_Process();
    assert(strcmp(reply, "0008 ok dyp distance_mm 742\r\n") == 0);

    (void)strcpy(reply, "unchanged");
    dyp_snapshot.state = SENSOR_SERVICE_DYP_IDLE;
    CommandService_Handle("sensor dyp", capture_reply, capture_io_error);
    assert(strcmp(reply, "unchanged") == 0);
    dyp_snapshot.state = SENSOR_SERVICE_DYP_COMPLETE;
    dyp_snapshot.distance_mm = 272U;
    CommandService_Process();
    assert(strcmp(reply, "ok dyp distance_mm 272\r\n") == 0);
}

static void test_dyp_errors_and_recovery(void)
{
    reset_dyp_command_state();
    next_dyp_start_status = SENSOR_SERVICE_STATUS_NOT_READY;
    CommandService_Handle("0010 sensor dyp", capture_reply, capture_io_error);
    assert(strcmp(reply, "0010 err not_ready\r\n") == 0);

    CommandService_Handle("0011 sensor mpu", capture_reply, capture_io_error);
    assert(strstr(reply, "0011 ok ") == reply);

    next_dyp_start_status = SENSOR_SERVICE_STATUS_OK;
    dyp_snapshot.state = SENSOR_SERVICE_DYP_IDLE;
    CommandService_Handle("0012 sensor dyp", capture_reply, capture_io_error);
    dyp_snapshot.state = SENSOR_SERVICE_DYP_TIMEOUT;
    CommandService_Process();
    assert(strcmp(reply, "0012 err timeout\r\n") == 0);

    dyp_snapshot.state = SENSOR_SERVICE_DYP_IDLE;
    CommandService_Handle("0013 sensor dyp", capture_reply, capture_io_error);
    dyp_snapshot.state = SENSOR_SERVICE_DYP_IO_ERROR;
    CommandService_Process();
    assert(strcmp(reply, "0013 err io\r\n") == 0);
    assert(reported_hal_error == dyp_hal_error);

    dyp_snapshot.state = SENSOR_SERVICE_DYP_IDLE;
    CommandService_Handle("0014 sensor dyp", capture_reply, capture_io_error);
    dyp_snapshot.state = SENSOR_SERVICE_DYP_COMPLETE;
    dyp_snapshot.distance_mm = 100U;
    CommandService_Process();
    assert(strcmp(reply, "0014 ok dyp distance_mm 100\r\n") == 0);
}

static void test_dyp_pending_io_callback_isolation(void)
{
    reset_dyp_command_state();
    CommandService_Handle("0008 sensor dyp", capture_reply,
                          capture_io_error_a);

    /* This synchronous command updates the service's current callback to B. */
    CommandService_Handle("0009 sensor all", capture_reply,
                          capture_io_error_b);
    dyp_snapshot.state = SENSOR_SERVICE_DYP_IO_ERROR;
    CommandService_Process();

    assert(strcmp(reply, "0008 err io\r\n") == 0);
    assert(reported_hal_error_a == dyp_hal_error);
    assert(reported_hal_error_b == 0U);
}

static void test_sensor_all_is_cached_only(void)
{
    reset_dyp_command_state();
    dyp_snapshot.state = SENSOR_SERVICE_DYP_IDLE;
    dyp_snapshot.valid = 0U;
    CommandService_Handle("0015 sensor all", capture_reply, capture_io_error);
    assert(dyp_start_count == 0U);
    assert(strstr(reply,
                  "0015 ok sensors mpu ready dyp ready state idle busy 0") == reply);
    assert(strstr(reply, "valid 0 distance_mm invalid age_ms invalid") != NULL);

    dyp_snapshot.state = SENSOR_SERVICE_DYP_COMPLETE;
    dyp_snapshot.valid = 1U;
    dyp_snapshot.distance_mm = 742U;
    dyp_snapshot.age_ms = 25U;
    CommandService_Handle("0016 sensor all", capture_reply, capture_io_error);
    assert(dyp_start_count == 0U);
    assert(strstr(reply, "valid 1 distance_mm 742 age_ms 25") != NULL);
}

int main(void)
{
    CommandService_Init();
    next_propeller_status = PROPELLER_SERVICE_STATUS_OK;
    test_unified_sequence();
    test_errors_and_removed_commands();
    test_stop_and_queries();
    test_dyp_pending_sequence();
    test_dyp_errors_and_recovery();
    test_dyp_pending_io_callback_isolation();
    test_sensor_all_is_cached_only();
    puts("command_service_test: PASS");
    return 0;
}
