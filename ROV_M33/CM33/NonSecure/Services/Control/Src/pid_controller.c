/**
  ******************************************************************************
  * @file    pid_controller.c
  * @brief   Generic floating-point PID controller with continuous deadband.
  ******************************************************************************
  */

#include "pid_controller.h"

#include <stddef.h>
#include <string.h>

static float PID_Controller_Clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

PID_Controller_StatusTypeDef PID_Controller_Init(
    PID_ControllerTypeDef *controller,
    const PID_Controller_ConfigTypeDef *config)
{
    if ((controller == NULL) || (config == NULL) ||
        (config->kp < 0.0f) || (config->ki < 0.0f) ||
        (config->kd < 0.0f) || (config->threshold < 0.0f) ||
        (config->integral_min > config->integral_max) ||
        (config->output_min > config->output_max))
    {
        return PID_CONTROLLER_STATUS_BAD_ARG;
    }

    memset(controller, 0, sizeof(*controller));
    controller->config = *config;
    controller->initialized = 1U;
    return PID_CONTROLLER_STATUS_OK;
}

void PID_Controller_Reset(PID_ControllerTypeDef *controller)
{
    if (controller == NULL)
    {
        return;
    }
    controller->integral_term = 0.0f;
    controller->previous_measurement = 0.0f;
    controller->has_previous_measurement = 0U;
}

PID_Controller_StatusTypeDef PID_Controller_Update(
    PID_ControllerTypeDef *controller,
    float setpoint,
    float measurement,
    float dt_seconds,
    float *effective_error,
    float *output)
{
    float error;
    float candidate_integral;
    float derivative = 0.0f;
    float proportional;
    float unsaturated;
    uint8_t reject_integral;

    if ((controller == NULL) || (effective_error == NULL) ||
        (output == NULL) || (dt_seconds <= 0.0f))
    {
        return PID_CONTROLLER_STATUS_BAD_ARG;
    }
    if (controller->initialized == 0U)
    {
        return PID_CONTROLLER_STATUS_NOT_READY;
    }

    error = setpoint - measurement;
    if (error > controller->config.threshold)
    {
        *effective_error = error - controller->config.threshold;
    }
    else if (error < -controller->config.threshold)
    {
        *effective_error = error + controller->config.threshold;
    }
    else
    {
        *effective_error = 0.0f;
    }

    if (*effective_error == 0.0f)
    {
        controller->integral_term = 0.0f;
        controller->previous_measurement = measurement;
        controller->has_previous_measurement = 1U;
        *output = 0.0f;
        return PID_CONTROLLER_STATUS_OK;
    }

    if (controller->has_previous_measurement != 0U)
    {
        derivative = -controller->config.kd *
            ((measurement - controller->previous_measurement) / dt_seconds);
    }
    controller->previous_measurement = measurement;
    controller->has_previous_measurement = 1U;

    proportional = controller->config.kp * (*effective_error);
    candidate_integral = PID_Controller_Clamp(
        controller->integral_term +
            (controller->config.ki * (*effective_error) * dt_seconds),
        controller->config.integral_min,
        controller->config.integral_max);
    unsaturated = proportional + candidate_integral + derivative;

    reject_integral =
        ((unsaturated > controller->config.output_max) &&
         (*effective_error > 0.0f)) ||
        ((unsaturated < controller->config.output_min) &&
         (*effective_error < 0.0f));
    if (reject_integral == 0U)
    {
        controller->integral_term = candidate_integral;
    }

    unsaturated = proportional + controller->integral_term + derivative;
    *output = PID_Controller_Clamp(unsaturated,
                                   controller->config.output_min,
                                   controller->config.output_max);
    return PID_CONTROLLER_STATUS_OK;
}
