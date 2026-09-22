/**
  ******************************************************************************
  * @file    servo.h
  * @brief   Servo service for LU9685 channels CH0..CH9.
  ******************************************************************************
  */

#ifndef SERVO_H
#define SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lu9685.h"

typedef enum
{
    SERVO_LEFT_SHOULDER = 0,
    SERVO_LEFT_UPPER = 1,
    SERVO_LEFT_ELBOW = 2,
    SERVO_LEFT_FRONT = 3,
    SERVO_LEFT_CLAW = 4,
    SERVO_RIGHT_SHOULDER = 5,
    SERVO_RIGHT_UPPER = 6,
    SERVO_RIGHT_ELBOW = 7,
    SERVO_RIGHT_FRONT = 8,
    SERVO_RIGHT_CLAW = 9
} ServoChannel;

#define SERVO_CHANNEL_FIRST       ((uint8_t)SERVO_LEFT_SHOULDER)
#define SERVO_CHANNEL_LAST        ((uint8_t)SERVO_RIGHT_CLAW)
#define SERVO_CHANNEL_COUNT       10U
#define SERVO_MIN_ANGLE           0U
#define SERVO_MID_ANGLE           90U
#define SERVO_MAX_ANGLE           180U

typedef enum
{
    SERVO_STATUS_OK = 0,
    SERVO_STATUS_BAD_ARG,
    SERVO_STATUS_NOT_READY,
    SERVO_STATUS_IO_ERROR
} Servo_StatusTypeDef;

Servo_StatusTypeDef servo_init(LU9685_HandleTypeDef *device);
Servo_StatusTypeDef servo_set(uint8_t id, uint8_t angle);
Servo_StatusTypeDef servo_set_all(uint8_t angle);
/* Returns the last target angle successfully submitted to LU9685, not feedback. */
Servo_StatusTypeDef servo_get(uint8_t id, uint8_t *angle);
/* Returns the last successfully submitted target angles, not position feedback. */
Servo_StatusTypeDef servo_get_all(uint8_t angles[SERVO_CHANNEL_COUNT]);
Servo_StatusTypeDef servo_mid(uint8_t id);
Servo_StatusTypeDef servo_mid_all(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_H */
