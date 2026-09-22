/**
  ******************************************************************************
  * @file    stability_control.h
  * @brief   Roll/pitch stabilization coordinator for vertical thrusters.
  ******************************************************************************
  */

#ifndef STABILITY_CONTROL_H
#define STABILITY_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "propeller_service.h"

typedef enum
{
    STABILITY_CONTROL_STATUS_OK = 0,
    STABILITY_CONTROL_STATUS_BAD_ARG,
    STABILITY_CONTROL_STATUS_NOT_READY,
    STABILITY_CONTROL_STATUS_SAFETY,
    STABILITY_CONTROL_STATUS_IO_ERROR
} StabilityControl_StatusTypeDef;

typedef struct
{
    float roll_deg;
    float pitch_deg;
    float roll_error_deg;
    float pitch_error_deg;
    float roll_pid_command;
    float pitch_pid_command;
    int16_t vertical_commands[4];
    uint8_t attitude_ready;
    uint8_t attitude_fresh;
    uint8_t horizontal_enabled;
    uint8_t global_stopped;
    uint8_t vertical_stopped;
    uint8_t horizontal_stopped;
} StabilityControl_TelemetryTypeDef;

StabilityControl_StatusTypeDef StabilityControl_Init(void);
void StabilityControl_Reset(void);
StabilityControl_StatusTypeDef StabilityControl_SyncState(
    const PropellerService_StateTypeDef *propeller_state);
StabilityControl_StatusTypeDef StabilityControl_Execute(
    float roll_deg,
    float pitch_deg,
    uint8_t attitude_ready,
    uint8_t attitude_fresh,
    const PropellerService_StateTypeDef *propeller_state,
    float dt_seconds);
StabilityControl_StatusTypeDef StabilityControl_GetTelemetry(
    StabilityControl_TelemetryTypeDef *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* STABILITY_CONTROL_H */
