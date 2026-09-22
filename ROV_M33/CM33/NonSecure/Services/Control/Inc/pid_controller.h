/**
  ******************************************************************************
  * @file    pid_controller.h
  * @brief   Generic floating-point PID controller with continuous deadband.
  ******************************************************************************
  */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    PID_CONTROLLER_STATUS_OK = 0,
    PID_CONTROLLER_STATUS_BAD_ARG,
    PID_CONTROLLER_STATUS_NOT_READY
} PID_Controller_StatusTypeDef;

typedef struct
{
    float kp;
    float ki;
    float kd;
    float threshold;
    float integral_min;
    float integral_max;
    float output_min;
    float output_max;
} PID_Controller_ConfigTypeDef;

typedef struct
{
    PID_Controller_ConfigTypeDef config;
    float integral_term;
    float previous_measurement;
    uint8_t has_previous_measurement;
    uint8_t initialized;
} PID_ControllerTypeDef;

PID_Controller_StatusTypeDef PID_Controller_Init(
    PID_ControllerTypeDef *controller,
    const PID_Controller_ConfigTypeDef *config);
void PID_Controller_Reset(PID_ControllerTypeDef *controller);
PID_Controller_StatusTypeDef PID_Controller_Update(
    PID_ControllerTypeDef *controller,
    float setpoint,
    float measurement,
    float dt_seconds,
    float *effective_error,
    float *output);

#ifdef __cplusplus
}
#endif

#endif /* PID_CONTROLLER_H */
