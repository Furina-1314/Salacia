/**
  ******************************************************************************
  * @file    servo.c
  * @brief   Servo service for LU9685 channels CH0..CH9.
  ******************************************************************************
  */

#include "servo.h"

#include "lu9685_channel_state.h"

static LU9685_HandleTypeDef *servo_device;

static Servo_StatusTypeDef Servo_FromLU9685Status(LU9685_StatusTypeDef status)
{
    switch (status)
    {
        case LU9685_STATUS_OK:
            return SERVO_STATUS_OK;
        case LU9685_STATUS_BAD_ARG:
            return SERVO_STATUS_BAD_ARG;
        case LU9685_STATUS_NOT_READY:
            return SERVO_STATUS_NOT_READY;
        default:
            return SERVO_STATUS_IO_ERROR;
    }
}

static uint8_t Servo_IsValidId(uint8_t id)
{
    return (id >= SERVO_CHANNEL_FIRST) && (id <= SERVO_CHANNEL_LAST);
}

Servo_StatusTypeDef servo_init(LU9685_HandleTypeDef *device)
{
    if ((device == NULL) || (device->initialized == 0U))
    {
        return SERVO_STATUS_BAD_ARG;
    }

    servo_device = device;
    LU9685_ChannelState_Init();

    return SERVO_STATUS_OK;
}

Servo_StatusTypeDef servo_set(uint8_t id, uint8_t angle)
{
    LU9685_StatusTypeDef status;

    if ((Servo_IsValidId(id) == 0U) || (angle > SERVO_MAX_ANGLE))
    {
        return SERVO_STATUS_BAD_ARG;
    }

    if (servo_device == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    status = LU9685_SetChannel(servo_device, id, angle);
    if (status == LU9685_STATUS_OK)
    {
        LU9685_ChannelState_CommitServo(id, angle);
    }

    return Servo_FromLU9685Status(status);
}

Servo_StatusTypeDef servo_set_all(uint8_t angle)
{
    uint8_t snapshot[LU9685_CHANNEL_COUNT];
    uint8_t channel;
    LU9685_StatusTypeDef status;

    if (angle > SERVO_MAX_ANGLE)
    {
        return SERVO_STATUS_BAD_ARG;
    }

    if (servo_device == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    LU9685_ChannelState_Copy(snapshot);
    for (channel = SERVO_CHANNEL_FIRST;
         channel <= SERVO_CHANNEL_LAST;
         ++channel)
    {
        snapshot[channel] = angle;
    }

    /* The 0xFD command always contains all 20 channel values. */
    status = LU9685_CommitAll(servo_device, snapshot);
    if (status == LU9685_STATUS_OK)
    {
        LU9685_ChannelState_CommitAllServos(angle);
    }

    return Servo_FromLU9685Status(status);
}

Servo_StatusTypeDef servo_get(uint8_t id, uint8_t *angle)
{
    if ((Servo_IsValidId(id) == 0U) || (angle == NULL))
    {
        return SERVO_STATUS_BAD_ARG;
    }

    if (servo_device == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    /* Software command state only; LU9685 provides no mechanical feedback. */
    *angle = LU9685_ChannelState_Get(id);
    return SERVO_STATUS_OK;
}

Servo_StatusTypeDef servo_get_all(uint8_t angles[SERVO_CHANNEL_COUNT])
{
    uint8_t channel;

    if (angles == NULL)
    {
        return SERVO_STATUS_BAD_ARG;
    }

    if (servo_device == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    for (channel = SERVO_CHANNEL_FIRST;
         channel <= SERVO_CHANNEL_LAST;
         ++channel)
    {
        angles[channel] = LU9685_ChannelState_Get(channel);
    }

    return SERVO_STATUS_OK;
}

Servo_StatusTypeDef servo_mid(uint8_t id)
{
    return servo_set(id, SERVO_MID_ANGLE);
}

Servo_StatusTypeDef servo_mid_all(void)
{
    return servo_set_all(SERVO_MID_ANGLE);
}
