/**
  ******************************************************************************
  * @file    command_service.c
  * @brief   Unified RPMsg command envelope parser and dispatcher.
  ******************************************************************************
  */

#include "command_service.h"

#include "actuator_service.h"
#include "attitude_estimator.h"
#include "propeller_service.h"
#include "sensor_service.h"
#include "stability_control.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define COMMAND_SEQUENCE_LENGTH 4U
#define COMMAND_REPLY_SIZE      192U

typedef struct
{
    char sequence[COMMAND_SEQUENCE_LENGTH + 1U];
    const char *payload;
} CommandEnvelopeTypeDef;

static CommandService_IoErrorCallback command_io_error_callback;

typedef struct
{
    char sequence[COMMAND_SEQUENCE_LENGTH + 1U];
    CommandService_SendCallback send_callback;
    CommandService_IoErrorCallback io_error_callback;
    uint8_t active;
} CommandPendingDypTypeDef;

static CommandPendingDypTypeDef command_pending_dyp;

static int32_t CommandScale100(float value)
{
    float scaled = value * 100.0f;

    return (int32_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

static int CommandHasOnlyTrailingSpace(const char *command, int consumed)
{
    if ((command == NULL) || (consumed < 0))
    {
        return 0;
    }
    while (command[consumed] != '\0')
    {
        if (isspace((unsigned char)command[consumed]) == 0)
        {
            return 0;
        }
        ++consumed;
    }
    return 1;
}

static CommandEnvelopeTypeDef CommandEnvelope_Parse(const char *message)
{
    CommandEnvelopeTypeDef envelope;
    uint8_t index;

    envelope.sequence[0] = '\0';
    envelope.payload = (message != NULL) ? message : "";
    if (message == NULL)
    {
        return envelope;
    }

    for (index = 0U; index < COMMAND_SEQUENCE_LENGTH; ++index)
    {
        if (isdigit((unsigned char)message[index]) == 0)
        {
            return envelope;
        }
    }
    if (isspace((unsigned char)message[COMMAND_SEQUENCE_LENGTH]) == 0)
    {
        return envelope;
    }

    memcpy(envelope.sequence, message, COMMAND_SEQUENCE_LENGTH);
    envelope.sequence[COMMAND_SEQUENCE_LENGTH] = '\0';
    envelope.payload = &message[COMMAND_SEQUENCE_LENGTH + 1U];
    while ((*envelope.payload != '\0') &&
           (isspace((unsigned char)*envelope.payload) != 0))
    {
        ++envelope.payload;
    }
    return envelope;
}

static void CommandSendResult(const CommandEnvelopeTypeDef *envelope,
                              const char *result,
                              CommandService_SendCallback send_callback)
{
    char reply[COMMAND_REPLY_SIZE];

    if (envelope->sequence[0] != '\0')
    {
        (void)snprintf(reply, sizeof(reply), "%s %s\r\n",
                       envelope->sequence, result);
    }
    else
    {
        (void)snprintf(reply, sizeof(reply), "%s\r\n", result);
    }
    send_callback(reply);
}

static void CommandSendActuatorStatus(
    const CommandEnvelopeTypeDef *envelope,
    ActuatorService_StatusTypeDef status,
    const char *success_result,
    CommandService_SendCallback send_callback)
{
    if (status == ACTUATOR_SERVICE_STATUS_OK)
    {
        CommandSendResult(envelope, success_result, send_callback);
    }
    else if (status == ACTUATOR_SERVICE_STATUS_BAD_ARG)
    {
        CommandSendResult(envelope, "err bad_arg", send_callback);
    }
    else if (status == ACTUATOR_SERVICE_STATUS_NOT_READY)
    {
        CommandSendResult(envelope, "err not_ready", send_callback);
    }
    else
    {
        if (command_io_error_callback != NULL)
        {
            command_io_error_callback(ActuatorService_GetLastHalError());
        }
        CommandSendResult(envelope, "err io", send_callback);
    }
}

static void CommandSendPropellerStatus(
    const CommandEnvelopeTypeDef *envelope,
    PropellerService_StatusTypeDef status,
    const char *success_result,
    CommandService_SendCallback send_callback)
{
    if (status == PROPELLER_SERVICE_STATUS_OK)
    {
        CommandSendResult(envelope, success_result, send_callback);
    }
    else if (status == PROPELLER_SERVICE_STATUS_BAD_ARG)
    {
        CommandSendResult(envelope, "err bad_arg", send_callback);
    }
    else if (status == PROPELLER_SERVICE_STATUS_SAFETY)
    {
        CommandSendResult(envelope, "err safety", send_callback);
    }
    else if (status == PROPELLER_SERVICE_STATUS_NOT_READY)
    {
        CommandSendResult(envelope, "err not_ready", send_callback);
    }
    else
    {
        if (command_io_error_callback != NULL)
        {
            command_io_error_callback(ActuatorService_GetLastHalError());
        }
        CommandSendResult(envelope, "err io", send_callback);
    }
}

static void CommandHandleSensorMpu(const CommandEnvelopeTypeDef *envelope,
                                   CommandService_SendCallback send_callback)
{
    MPU6500_RawData data;
    SensorService_StatusTypeDef status;
    char result[80];

    status = SensorService_ReadMpuRaw(&data);
    if (status == SENSOR_SERVICE_STATUS_OK)
    {
        (void)snprintf(result, sizeof(result), "ok %d %d %d %d %d %d",
                       data.ax, data.ay, data.az, data.gx, data.gy, data.gz);
        CommandSendResult(envelope, result, send_callback);
    }
    else if (status == SENSOR_SERVICE_STATUS_BAD_ARG)
    {
        CommandSendResult(envelope, "err bad_arg", send_callback);
    }
    else if (status == SENSOR_SERVICE_STATUS_BUSY)
    {
        CommandSendResult(envelope, "err busy", send_callback);
    }
    else if (status == SENSOR_SERVICE_STATUS_TIMEOUT)
    {
        CommandSendResult(envelope, "err timeout", send_callback);
    }
    else
    {
        CommandSendResult(envelope, "err not_ready", send_callback);
    }
}

static const char *CommandDypStateName(SensorService_DypStateTypeDef state)
{
    switch (state)
    {
        case SENSOR_SERVICE_DYP_IDLE:
            return "idle";

        case SENSOR_SERVICE_DYP_WAITING:
            return "waiting";

        case SENSOR_SERVICE_DYP_COMPLETE:
            return "complete";

        case SENSOR_SERVICE_DYP_TIMEOUT:
            return "timeout";

        case SENSOR_SERVICE_DYP_IO_ERROR:
            return "io_error";

        default:
            return "uninitialized";
    }
}

static void CommandHandleSensorDyp(const CommandEnvelopeTypeDef *envelope,
                                   CommandService_SendCallback send_callback)
{
    SensorService_StatusTypeDef status;

    if (command_pending_dyp.active != 0U)
    {
        CommandSendResult(envelope, "err busy", send_callback);
        return;
    }

    status = SensorService_StartDypMeasurement();
    if (status == SENSOR_SERVICE_STATUS_OK)
    {
        memcpy(command_pending_dyp.sequence, envelope->sequence,
               sizeof(command_pending_dyp.sequence));
        command_pending_dyp.send_callback = send_callback;
        command_pending_dyp.io_error_callback = command_io_error_callback;
        command_pending_dyp.active = 1U;
        return;
    }
    if (status == SENSOR_SERVICE_STATUS_BUSY)
    {
        CommandSendResult(envelope, "err busy", send_callback);
    }
    else if (status == SENSOR_SERVICE_STATUS_TIMEOUT)
    {
        CommandSendResult(envelope, "err timeout", send_callback);
    }
    else if (status == SENSOR_SERVICE_STATUS_IO_ERROR)
    {
        if (command_io_error_callback != NULL)
        {
            command_io_error_callback(SensorService_GetDypLastHalError());
        }
        CommandSendResult(envelope, "err io", send_callback);
    }
    else
    {
        CommandSendResult(envelope, "err not_ready", send_callback);
    }
}

static void CommandHandleSensorAll(const CommandEnvelopeTypeDef *envelope,
                                   CommandService_SendCallback send_callback)
{
    SensorService_DypSnapshotTypeDef snapshot;
    SensorService_StatusTypeDef status;
    const char *mpu_state;
    const char *dyp_state;
    char result[COMMAND_REPLY_SIZE];

    status = SensorService_GetDypSnapshot(&snapshot);
    if (status != SENSOR_SERVICE_STATUS_OK)
    {
        CommandSendResult(envelope, "err io", send_callback);
        return;
    }

    mpu_state = (SensorService_IsMpuReady() != 0U) ? "ready" : "not_ready";
    dyp_state = (snapshot.ready != 0U) ? "ready" : "not_ready";
    if (snapshot.valid != 0U)
    {
        (void)snprintf(result, sizeof(result),
            "ok sensors mpu %s dyp %s state %s busy %u valid 1 distance_mm %u age_ms %lu",
            mpu_state, dyp_state, CommandDypStateName(snapshot.state),
            (unsigned int)snapshot.busy,
            (unsigned int)snapshot.distance_mm,
            (unsigned long)snapshot.age_ms);
    }
    else
    {
        (void)snprintf(result, sizeof(result),
            "ok sensors mpu %s dyp %s state %s busy %u valid 0 distance_mm invalid age_ms invalid",
            mpu_state, dyp_state, CommandDypStateName(snapshot.state),
            (unsigned int)snapshot.busy);
    }
    CommandSendResult(envelope, result, send_callback);
}

static void CommandFormatPropellerAll(char *result,
                                      size_t result_size,
                                      const char *kind,
                                      const int16_t commands[6])
{
    (void)snprintf(result, result_size,
                   "ok propeller all %s %d %d %d %d %d %d",
                   kind,
                   (int)commands[0], (int)commands[1], (int)commands[2],
                   (int)commands[3], (int)commands[4], (int)commands[5]);
}

void CommandService_Init(void)
{
    memset(&command_pending_dyp, 0, sizeof(command_pending_dyp));
    command_io_error_callback = NULL;
}

void CommandService_Handle(const char *message,
                           CommandService_SendCallback send_callback,
                           CommandService_IoErrorCallback io_error_callback)
{
    CommandEnvelopeTypeDef envelope;
    const char *payload;
    unsigned int id;
    unsigned int angle;
    int value;
    int consumed = 0;
    char result[COMMAND_REPLY_SIZE];
    uint8_t angles[ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT];
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT] = {0};
    ActuatorService_StatusTypeDef actuator_status;
    PropellerService_StatusTypeDef propeller_status;
    size_t used;
    uint8_t channel;
    AttitudeEstimator_StateTypeDef attitude;
    StabilityControl_TelemetryTypeDef telemetry;

    if (send_callback == NULL)
    {
        return;
    }
    command_io_error_callback = io_error_callback;
    envelope = CommandEnvelope_Parse(message);
    payload = envelope.payload;

    if (strcmp(payload, "stop") == 0)
    {
        CommandSendPropellerStatus(&envelope, PropellerService_StopAll(),
                                   "ok", send_callback);
        return;
    }
    if (strcmp(payload, "move") == 0)
    {
        CommandSendPropellerStatus(&envelope, PropellerService_MoveAll(),
                                   "ok", send_callback);
        return;
    }
    if (strcmp(payload, "stop vertical") == 0)
    {
        CommandSendPropellerStatus(&envelope,
                                   PropellerService_StopVertical(),
                                   "ok", send_callback);
        return;
    }
    if (strcmp(payload, "move vertical") == 0)
    {
        CommandSendPropellerStatus(&envelope,
                                   PropellerService_MoveVertical(),
                                   "ok", send_callback);
        return;
    }
    if (strcmp(payload, "stop horizontal") == 0)
    {
        CommandSendPropellerStatus(&envelope,
                                   PropellerService_StopHorizontal(),
                                   "ok", send_callback);
        return;
    }
    if (strcmp(payload, "move horizontal") == 0)
    {
        CommandSendPropellerStatus(&envelope,
                                   PropellerService_MoveHorizontal(),
                                   "ok", send_callback);
        return;
    }

    if (strcmp(payload, "get attitude") == 0)
    {
        if (AttitudeEstimator_GetState(&attitude) !=
            ATTITUDE_ESTIMATOR_STATUS_OK)
        {
            CommandSendResult(&envelope, "err not_ready", send_callback);
            return;
        }
        (void)snprintf(result, sizeof(result),
                       "ok attitude roll %ld pitch %ld ready %u",
                       (long)CommandScale100(attitude.roll_deg),
                       (long)CommandScale100(attitude.pitch_deg),
                       (unsigned int)attitude.ready);
        CommandSendResult(&envelope, result, send_callback);
        return;
    }
    if (strcmp(payload, "get stabilization") == 0)
    {
        if (StabilityControl_GetTelemetry(&telemetry) !=
            STABILITY_CONTROL_STATUS_OK)
        {
            CommandSendResult(&envelope, "err not_ready", send_callback);
            return;
        }
        (void)snprintf(result, sizeof(result),
            "ok stabilization re %ld pe %ld rp %ld pp %ld ch %d %d %d %d ar %u af %u he %u gs %u vs %u hs %u",
            (long)CommandScale100(telemetry.roll_error_deg),
            (long)CommandScale100(telemetry.pitch_error_deg),
            (long)CommandScale100(telemetry.roll_pid_command),
            (long)CommandScale100(telemetry.pitch_pid_command),
            (int)telemetry.vertical_commands[0],
            (int)telemetry.vertical_commands[1],
            (int)telemetry.vertical_commands[2],
            (int)telemetry.vertical_commands[3],
            (unsigned int)telemetry.attitude_ready,
            (unsigned int)telemetry.attitude_fresh,
            (unsigned int)telemetry.horizontal_enabled,
            (unsigned int)telemetry.global_stopped,
            (unsigned int)telemetry.vertical_stopped,
            (unsigned int)telemetry.horizontal_stopped);
        CommandSendResult(&envelope, result, send_callback);
        return;
    }

    if (strcmp(payload, "horizontal on") == 0)
    {
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetHorizontalEnabled(1U), "ok", send_callback);
        return;
    }
    if (strcmp(payload, "horizontal off") == 0)
    {
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetHorizontalEnabled(0U), "ok", send_callback);
        return;
    }
    if (strcmp(payload, "synchronization on") == 0)
    {
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetSynchronizationEnabled(1U), "ok", send_callback);
        return;
    }
    if (strcmp(payload, "synchronization off") == 0)
    {
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetSynchronizationEnabled(0U), "ok", send_callback);
        return;
    }

    consumed = 0;
    if ((sscanf(payload, "set propeller vertical base %d %n",
                &value, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((value < PROPELLER_SERVICE_COMMAND_MIN) ||
            (value > PROPELLER_SERVICE_COMMAND_MAX))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetVerticalBase((int16_t)value), "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "set propeller vertical %u %d %n",
                &id, &value, &consumed) == 2) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((id < PROPELLER_SERVICE_VERTICAL_FIRST) ||
            (id > PROPELLER_SERVICE_VERTICAL_LAST) ||
            (value < PROPELLER_SERVICE_COMMAND_MIN) ||
            (value > PROPELLER_SERVICE_COMMAND_MAX))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetVertical((uint8_t)id, (int16_t)value),
            "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "set propeller horizontal base %d %n",
                &value, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((value < PROPELLER_SERVICE_COMMAND_MIN) ||
            (value > PROPELLER_SERVICE_COMMAND_MAX))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetHorizontalBase((int16_t)value), "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "set propeller horizontal %u %d %n",
                &id, &value, &consumed) == 2) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((id < PROPELLER_SERVICE_HORIZONTAL_FIRST) ||
            (id > PROPELLER_SERVICE_HORIZONTAL_LAST) ||
            (value < PROPELLER_SERVICE_COMMAND_MIN) ||
            (value > PROPELLER_SERVICE_COMMAND_MAX))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendPropellerStatus(&envelope,
            PropellerService_SetHorizontal((uint8_t)id, (int16_t)value),
            "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "get propeller %u base %n", &id, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((id < PROPELLER_SERVICE_CHANNEL_FIRST) ||
            (id > PROPELLER_SERVICE_CHANNEL_LAST))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        propeller_status = PropellerService_GetBase((uint8_t)id, &commands[0]);
        (void)snprintf(result, sizeof(result), "ok propeller %u base %d",
                       id, (int)commands[0]);
        CommandSendPropellerStatus(&envelope, propeller_status,
                                   result, send_callback);
        return;
    }
    if (strcmp(payload, "get propeller all base") == 0)
    {
        propeller_status = PropellerService_GetAllBase(commands);
        CommandFormatPropellerAll(result, sizeof(result), "base", commands);
        CommandSendPropellerStatus(&envelope, propeller_status,
                                   result, send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "get propeller %u real %n", &id, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((id < PROPELLER_SERVICE_CHANNEL_FIRST) ||
            (id > PROPELLER_SERVICE_CHANNEL_LAST))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        propeller_status = PropellerService_GetReal((uint8_t)id, &commands[0]);
        (void)snprintf(result, sizeof(result), "ok propeller %u real %d",
                       id, (int)commands[0]);
        CommandSendPropellerStatus(&envelope, propeller_status,
                                   result, send_callback);
        return;
    }
    if (strcmp(payload, "get propeller all real") == 0)
    {
        propeller_status = PropellerService_GetAllReal(commands);
        CommandFormatPropellerAll(result, sizeof(result), "real", commands);
        CommandSendPropellerStatus(&envelope, propeller_status,
                                   result, send_callback);
        return;
    }

    if ((strncmp(payload, "set propeller", 13U) == 0) ||
        (strncmp(payload, "get propeller", 13U) == 0))
    {
        CommandSendResult(&envelope, "err bad_arg", send_callback);
        return;
    }

    if ((strncmp(payload, "sensor mpu", 10U) == 0) &&
        CommandHasOnlyTrailingSpace(payload, 10))
    {
        CommandHandleSensorMpu(&envelope, send_callback);
        return;
    }

    if (strcmp(payload, "sensor dyp") == 0)
    {
        CommandHandleSensorDyp(&envelope, send_callback);
        return;
    }
    if (strncmp(payload, "sensor dyp", 10U) == 0)
    {
        CommandSendResult(&envelope, "err bad_arg", send_callback);
        return;
    }
    if (strcmp(payload, "sensor all") == 0)
    {
        CommandHandleSensorAll(&envelope, send_callback);
        return;
    }
    if (strncmp(payload, "sensor all", 10U) == 0)
    {
        CommandSendResult(&envelope, "err bad_arg", send_callback);
        return;
    }
    if (strncmp(payload, "sensor mpu", 10U) == 0)
    {
        CommandSendResult(&envelope, "err bad_arg", send_callback);
        return;
    }

    if (strcmp(payload, "set servo all mid") == 0)
    {
        CommandSendActuatorStatus(&envelope, ActuatorService_SetServoAllMid(),
                                  "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "set servo %u mid %n", &id, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if (id > ACTUATOR_SERVICE_SERVO_CHANNEL_LAST)
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendActuatorStatus(&envelope,
            ActuatorService_SetServoMid((uint8_t)id), "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "set servo all %u %n", &angle, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if (angle > ACTUATOR_SERVICE_SERVO_MAX_ANGLE)
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendActuatorStatus(&envelope,
            ActuatorService_SetServoAll((uint8_t)angle), "ok", send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "set servo %u %u %n", &id, &angle, &consumed) == 2) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        if ((id > ACTUATOR_SERVICE_SERVO_CHANNEL_LAST) ||
            (angle > ACTUATOR_SERVICE_SERVO_MAX_ANGLE))
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        CommandSendActuatorStatus(&envelope,
            ActuatorService_SetServo((uint8_t)id, (uint8_t)angle),
            "ok", send_callback);
        return;
    }
    if (strcmp(payload, "get servo all") == 0)
    {
        actuator_status = ActuatorService_GetServoAll(angles);
        used = (size_t)snprintf(result, sizeof(result), "ok servo all");
        for (channel = 0U;
             (channel < ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT) &&
             (used < sizeof(result));
             ++channel)
        {
            used += (size_t)snprintf(&result[used], sizeof(result) - used,
                                     " %u", angles[channel]);
        }
        CommandSendActuatorStatus(&envelope, actuator_status,
                                  result, send_callback);
        return;
    }
    consumed = 0;
    if ((sscanf(payload, "get servo %u %n", &id, &consumed) == 1) &&
        CommandHasOnlyTrailingSpace(payload, consumed))
    {
        uint8_t current_angle = 0U;

        if (id > ACTUATOR_SERVICE_SERVO_CHANNEL_LAST)
        {
            CommandSendResult(&envelope, "err bad_arg", send_callback);
            return;
        }
        actuator_status = ActuatorService_GetServo((uint8_t)id, &current_angle);
        (void)snprintf(result, sizeof(result), "ok servo %u %u",
                       id, current_angle);
        CommandSendActuatorStatus(&envelope, actuator_status,
                                  result, send_callback);
        return;
    }
    if ((strncmp(payload, "set servo", 9U) == 0) ||
        (strncmp(payload, "get servo", 9U) == 0))
    {
        CommandSendResult(&envelope, "err bad_arg", send_callback);
        return;
    }

    CommandSendResult(&envelope, "err bad_cmd", send_callback);
}

void CommandService_Process(void)
{
    SensorService_DypSnapshotTypeDef snapshot;
    CommandEnvelopeTypeDef envelope;
    CommandService_SendCallback send_callback;
    CommandService_IoErrorCallback io_error_callback;
    char result[48];

    if (command_pending_dyp.active == 0U)
    {
        return;
    }
    if (SensorService_GetDypSnapshot(&snapshot) != SENSOR_SERVICE_STATUS_OK)
    {
        return;
    }

    if ((snapshot.state == SENSOR_SERVICE_DYP_WAITING) ||
        (snapshot.state == SENSOR_SERVICE_DYP_IDLE))
    {
        return;
    }

    memcpy(envelope.sequence, command_pending_dyp.sequence,
           sizeof(envelope.sequence));
    envelope.payload = "";
    send_callback = command_pending_dyp.send_callback;
    io_error_callback = command_pending_dyp.io_error_callback;
    command_pending_dyp.active = 0U;
    command_pending_dyp.send_callback = NULL;
    command_pending_dyp.io_error_callback = NULL;

    if (snapshot.state == SENSOR_SERVICE_DYP_COMPLETE)
    {
        (void)snprintf(result, sizeof(result),
                       "ok dyp distance_mm %u",
                       (unsigned int)snapshot.distance_mm);
        CommandSendResult(&envelope, result, send_callback);
    }
    else if (snapshot.state == SENSOR_SERVICE_DYP_TIMEOUT)
    {
        CommandSendResult(&envelope, "err timeout", send_callback);
    }
    else if (snapshot.state == SENSOR_SERVICE_DYP_IO_ERROR)
    {
        if (io_error_callback != NULL)
        {
            io_error_callback(SensorService_GetDypLastHalError());
        }
        CommandSendResult(&envelope, "err io", send_callback);
    }
    else
    {
        CommandSendResult(&envelope, "err not_ready", send_callback);
    }
}
