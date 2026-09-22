/**
  ******************************************************************************
  * @file    servo.c
  * @brief   Servo policy and target state for actuator channels CH0..CH9.
  ******************************************************************************
  */

#include "servo.h"

#include <stddef.h>

static Servo_SubmitPulseUsFunction servo_submit_pulse_us;
static uint8_t servo_angles[SERVO_CHANNEL_COUNT];

static uint8_t Servo_IsValidId(uint8_t id)
{
    return (id >= SERVO_CHANNEL_FIRST) && (id <= SERVO_CHANNEL_LAST);
}

Servo_StatusTypeDef servo_angle_to_pulse_us(uint8_t angle,
                                            uint16_t *pulse_us)
{
    uint32_t pulse_range;

    if ((pulse_us == NULL) || (angle > SERVO_MAX_ANGLE))
    {
        return SERVO_STATUS_BAD_ARG;
    }

    pulse_range = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    *pulse_us = (uint16_t)(SERVO_MIN_PULSE_US +
        ((((uint32_t)angle * pulse_range) + (SERVO_MAX_ANGLE / 2U)) /
         SERVO_MAX_ANGLE));
    return SERVO_STATUS_OK;
}

Servo_StatusTypeDef servo_init(Servo_SubmitPulseUsFunction submit_pulse_us)
{
    uint8_t channel;

    if (submit_pulse_us == NULL)
    {
        return SERVO_STATUS_BAD_ARG;
    }

    servo_submit_pulse_us = submit_pulse_us;
    for (channel = 0U; channel < SERVO_CHANNEL_COUNT; ++channel)
    {
        servo_angles[channel] = SERVO_MID_ANGLE;
    }
    return SERVO_STATUS_OK;
}

Servo_StatusTypeDef servo_set(uint8_t id, uint8_t angle)
{
    uint16_t pulse_us;
    Servo_StatusTypeDef status;

    if ((Servo_IsValidId(id) == 0U) || (angle > SERVO_MAX_ANGLE))
    {
        return SERVO_STATUS_BAD_ARG;
    }
    if (servo_submit_pulse_us == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    status = servo_angle_to_pulse_us(angle, &pulse_us);
    if (status == SERVO_STATUS_OK)
    {
        status = servo_submit_pulse_us(id, &pulse_us, 1U);
    }
    if (status == SERVO_STATUS_OK)
    {
        servo_angles[id] = angle;
    }
    return status;
}

Servo_StatusTypeDef servo_set_all(uint8_t angle)
{
    uint16_t pulse_us[SERVO_CHANNEL_COUNT];
    uint16_t mapped_pulse;
    uint8_t channel;
    Servo_StatusTypeDef status;

    if (angle > SERVO_MAX_ANGLE)
    {
        return SERVO_STATUS_BAD_ARG;
    }
    if (servo_submit_pulse_us == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    status = servo_angle_to_pulse_us(angle, &mapped_pulse);
    if (status != SERVO_STATUS_OK)
    {
        return status;
    }
    for (channel = 0U; channel < SERVO_CHANNEL_COUNT; ++channel)
    {
        pulse_us[channel] = mapped_pulse;
    }

    status = servo_submit_pulse_us(SERVO_CHANNEL_FIRST,
                                   pulse_us,
                                   SERVO_CHANNEL_COUNT);
    if (status == SERVO_STATUS_OK)
    {
        for (channel = 0U; channel < SERVO_CHANNEL_COUNT; ++channel)
        {
            servo_angles[channel] = angle;
        }
    }
    return status;
}

Servo_StatusTypeDef servo_get(uint8_t id, uint8_t *angle)
{
    if ((Servo_IsValidId(id) == 0U) || (angle == NULL))
    {
        return SERVO_STATUS_BAD_ARG;
    }
    if (servo_submit_pulse_us == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    *angle = servo_angles[id];
    return SERVO_STATUS_OK;
}

Servo_StatusTypeDef servo_get_all(uint8_t angles[SERVO_CHANNEL_COUNT])
{
    uint8_t channel;

    if (angles == NULL)
    {
        return SERVO_STATUS_BAD_ARG;
    }
    if (servo_submit_pulse_us == NULL)
    {
        return SERVO_STATUS_NOT_READY;
    }

    for (channel = 0U; channel < SERVO_CHANNEL_COUNT; ++channel)
    {
        angles[channel] = servo_angles[channel];
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
