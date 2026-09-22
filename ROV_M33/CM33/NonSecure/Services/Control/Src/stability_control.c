/**
  ******************************************************************************
  * @file    stability_control.c
  * @brief   Roll/pitch stabilization coordinator for vertical thrusters.
  ******************************************************************************
  */

#include "stability_control.h"

#include "pid_controller.h"
#include "vertical_mixer.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    PID_ControllerTypeDef roll_pid;
    PID_ControllerTypeDef pitch_pid;
    StabilityControl_TelemetryTypeDef telemetry;
    uint8_t initialized;
    uint8_t state_seen;
} StabilityControl_ContextTypeDef;

static StabilityControl_ContextTypeDef stability_control;

static StabilityControl_StatusTypeDef StabilityControl_MapPropellerStatus(
    PropellerService_StatusTypeDef status)
{
    switch (status)
    {
        case PROPELLER_SERVICE_STATUS_OK:
            return STABILITY_CONTROL_STATUS_OK;
        case PROPELLER_SERVICE_STATUS_BAD_ARG:
            return STABILITY_CONTROL_STATUS_BAD_ARG;
        case PROPELLER_SERVICE_STATUS_NOT_READY:
            return STABILITY_CONTROL_STATUS_NOT_READY;
        case PROPELLER_SERVICE_STATUS_SAFETY:
            return STABILITY_CONTROL_STATUS_SAFETY;
        default:
            return STABILITY_CONTROL_STATUS_IO_ERROR;
    }
}

static void StabilityControl_CopyReal(
    const PropellerService_StateTypeDef *state)
{
    uint8_t index;

    for (index = 0U; index < 4U; ++index)
    {
        stability_control.telemetry.vertical_commands[index] =
            state->real[index];
    }
}

StabilityControl_StatusTypeDef StabilityControl_Init(void)
{
    PID_Controller_ConfigTypeDef roll_config = {
        0.8f, 0.02f, 0.15f, 10.0f, -5.0f, 5.0f, -10.0f, 10.0f
    };
    PID_Controller_ConfigTypeDef pitch_config = {
        0.8f, 0.02f, 0.15f, 10.0f, -5.0f, 5.0f, -10.0f, 10.0f
    };

    memset(&stability_control, 0, sizeof(stability_control));
    if ((PID_Controller_Init(&stability_control.roll_pid, &roll_config) !=
         PID_CONTROLLER_STATUS_OK) ||
        (PID_Controller_Init(&stability_control.pitch_pid, &pitch_config) !=
         PID_CONTROLLER_STATUS_OK))
    {
        return STABILITY_CONTROL_STATUS_BAD_ARG;
    }
    stability_control.initialized = 1U;
    return STABILITY_CONTROL_STATUS_OK;
}

void StabilityControl_Reset(void)
{
    PID_Controller_Reset(&stability_control.roll_pid);
    PID_Controller_Reset(&stability_control.pitch_pid);
    stability_control.telemetry.roll_error_deg = 0.0f;
    stability_control.telemetry.pitch_error_deg = 0.0f;
    stability_control.telemetry.roll_pid_command = 0.0f;
    stability_control.telemetry.pitch_pid_command = 0.0f;
}

StabilityControl_StatusTypeDef StabilityControl_SyncState(
    const PropellerService_StateTypeDef *state)
{
    uint8_t changed;

    if (state == NULL)
    {
        return STABILITY_CONTROL_STATUS_BAD_ARG;
    }
    if (stability_control.initialized == 0U)
    {
        return STABILITY_CONTROL_STATUS_NOT_READY;
    }

    changed = (stability_control.state_seen == 0U) ||
        (stability_control.telemetry.horizontal_enabled !=
         state->horizontal_enabled) ||
        (stability_control.telemetry.global_stopped != state->global_stopped) ||
        (stability_control.telemetry.vertical_stopped !=
         state->vertical_stopped);
    stability_control.telemetry.horizontal_enabled = state->horizontal_enabled;
    stability_control.telemetry.global_stopped = state->global_stopped;
    stability_control.telemetry.vertical_stopped = state->vertical_stopped;
    stability_control.telemetry.horizontal_stopped = state->horizontal_stopped;
    StabilityControl_CopyReal(state);
    stability_control.state_seen = 1U;
    if (changed != 0U)
    {
        StabilityControl_Reset();
    }
    return STABILITY_CONTROL_STATUS_OK;
}

StabilityControl_StatusTypeDef StabilityControl_Execute(
    float roll_deg,
    float pitch_deg,
    uint8_t attitude_ready,
    uint8_t attitude_fresh,
    const PropellerService_StateTypeDef *state,
    float dt_seconds)
{
    int16_t commands[4];
    PropellerService_StatusTypeDef propeller_status;
    uint8_t blocked;
    uint8_t index;

    if ((state == NULL) || (dt_seconds <= 0.0f))
    {
        return STABILITY_CONTROL_STATUS_BAD_ARG;
    }
    if (stability_control.initialized == 0U)
    {
        return STABILITY_CONTROL_STATUS_NOT_READY;
    }

    (void)StabilityControl_SyncState(state);
    stability_control.telemetry.roll_deg = roll_deg;
    stability_control.telemetry.pitch_deg = pitch_deg;
    stability_control.telemetry.attitude_ready = attitude_ready;
    stability_control.telemetry.attitude_fresh = attitude_fresh;
    blocked = (state->global_stopped != 0U) ||
              (state->vertical_stopped != 0U);

    if ((state->horizontal_enabled == 0U) || (attitude_ready == 0U) ||
        (blocked != 0U))
    {
        StabilityControl_Reset();
        return (blocked != 0U) ? STABILITY_CONTROL_STATUS_SAFETY :
                                STABILITY_CONTROL_STATUS_OK;
    }

    if (attitude_fresh == 0U)
    {
        StabilityControl_Reset();
        for (index = 0U; index < 4U; ++index)
        {
            commands[index] = state->vertical_base;
        }
    }
    else
    {
        if ((PID_Controller_Update(&stability_control.roll_pid,
                                   0.0f, roll_deg, dt_seconds,
                                   &stability_control.telemetry.roll_error_deg,
                                   &stability_control.telemetry.roll_pid_command)
             != PID_CONTROLLER_STATUS_OK) ||
            (PID_Controller_Update(&stability_control.pitch_pid,
                                   0.0f, pitch_deg, dt_seconds,
                                   &stability_control.telemetry.pitch_error_deg,
                                   &stability_control.telemetry.pitch_pid_command)
             != PID_CONTROLLER_STATUS_OK) ||
            (VerticalMixer_Mix(
                 state->vertical_base,
                 stability_control.telemetry.roll_pid_command,
                 stability_control.telemetry.pitch_pid_command,
                 commands) != VERTICAL_MIXER_STATUS_OK))
        {
            StabilityControl_Reset();
            return STABILITY_CONTROL_STATUS_BAD_ARG;
        }
    }

    propeller_status = PropellerService_ApplyVerticalControl(commands);
    if (propeller_status != PROPELLER_SERVICE_STATUS_OK)
    {
        StabilityControl_Reset();
        StabilityControl_CopyReal(state);
        return StabilityControl_MapPropellerStatus(propeller_status);
    }
    for (index = 0U; index < 4U; ++index)
    {
        stability_control.telemetry.vertical_commands[index] = commands[index];
    }
    return STABILITY_CONTROL_STATUS_OK;
}

StabilityControl_StatusTypeDef StabilityControl_GetTelemetry(
    StabilityControl_TelemetryTypeDef *telemetry)
{
    if (telemetry == NULL)
    {
        return STABILITY_CONTROL_STATUS_BAD_ARG;
    }
    if (stability_control.initialized == 0U)
    {
        return STABILITY_CONTROL_STATUS_NOT_READY;
    }
    *telemetry = stability_control.telemetry;
    return STABILITY_CONTROL_STATUS_OK;
}
