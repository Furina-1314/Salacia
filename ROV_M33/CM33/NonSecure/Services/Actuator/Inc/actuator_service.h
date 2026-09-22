/**
  ******************************************************************************
  * @file    actuator_service.h
  * @brief   Actuator service for PCA9685-backed servos and thrusters.
  ******************************************************************************
  */

#ifndef ACTUATOR_SERVICE_H
#define ACTUATOR_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32mp2xx_hal.h"

#define ACTUATOR_SERVICE_CHANNEL_COUNT             16U
#define ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT       10U
#define ACTUATOR_SERVICE_SERVO_CHANNEL_LAST         9U
#define ACTUATOR_SERVICE_SERVO_MIN_ANGLE             0U
#define ACTUATOR_SERVICE_SERVO_MID_ANGLE            90U
#define ACTUATOR_SERVICE_SERVO_MAX_ANGLE            180U
#define ACTUATOR_SERVICE_SAFE_PULSE_US             1500U

typedef enum
{
    ACTUATOR_SERVICE_STATUS_OK = 0,
    ACTUATOR_SERVICE_STATUS_BAD_ARG,
    ACTUATOR_SERVICE_STATUS_NOT_READY,
    ACTUATOR_SERVICE_STATUS_IO_ERROR
} ActuatorService_StatusTypeDef;

ActuatorService_StatusTypeDef ActuatorService_Init(I2C_HandleTypeDef *i2c);
ActuatorService_StatusTypeDef ActuatorService_SetServo(uint8_t id,
                                                       uint8_t angle);
ActuatorService_StatusTypeDef ActuatorService_SetServoAll(uint8_t angle);
ActuatorService_StatusTypeDef ActuatorService_GetServo(uint8_t id,
                                                       uint8_t *angle);
ActuatorService_StatusTypeDef ActuatorService_GetServoAll(
    uint8_t angles[ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT]);
ActuatorService_StatusTypeDef ActuatorService_SetServoMid(uint8_t id);
ActuatorService_StatusTypeDef ActuatorService_SetServoAllMid(void);
/* Submit one contiguous range in microseconds; valid hardware channels are 0..15. */
ActuatorService_StatusTypeDef ActuatorService_SetChannelPulseUs(
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count);
uint32_t ActuatorService_GetLastHalError(void);

#ifdef __cplusplus
}
#endif

#endif /* ACTUATOR_SERVICE_H */
