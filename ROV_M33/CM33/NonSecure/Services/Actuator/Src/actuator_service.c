/**
  ******************************************************************************
  * @file    actuator_service.c
  * @brief   Actuator service for PCA9685-backed servos and thrusters.
  ******************************************************************************
  */

#include "actuator_service.h"

#include "pca9685.h"
#include "servo.h"

#include <string.h>

static PCA9685_HandleTypeDef pca9685;
static uint16_t actuator_pulse_us[ACTUATOR_SERVICE_CHANNEL_COUNT];

static ActuatorService_StatusTypeDef ActuatorService_MapPCA9685Status(
    PCA9685_StatusTypeDef status)
{
    switch (status)
    {
        case PCA9685_STATUS_OK:
            return ACTUATOR_SERVICE_STATUS_OK;
        case PCA9685_STATUS_BAD_ARG:
            return ACTUATOR_SERVICE_STATUS_BAD_ARG;
        case PCA9685_STATUS_NOT_READY:
            return ACTUATOR_SERVICE_STATUS_NOT_READY;
        default:
            return ACTUATOR_SERVICE_STATUS_IO_ERROR;
    }
}

static ActuatorService_StatusTypeDef ActuatorService_MapServoStatus(
    Servo_StatusTypeDef status)
{
    switch (status)
    {
        case SERVO_STATUS_OK:
            return ACTUATOR_SERVICE_STATUS_OK;
        case SERVO_STATUS_BAD_ARG:
            return ACTUATOR_SERVICE_STATUS_BAD_ARG;
        case SERVO_STATUS_NOT_READY:
            return ACTUATOR_SERVICE_STATUS_NOT_READY;
        default:
            return ACTUATOR_SERVICE_STATUS_IO_ERROR;
    }
}

static ActuatorService_StatusTypeDef ActuatorService_ApplyPulseRange(
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count)
{
    uint8_t index;
    uint16_t pwm_count;
    PCA9685_StatusTypeDef status;

    if ((pulse_us == NULL) || (count == 0U) ||
        (first_channel >= ACTUATOR_SERVICE_CHANNEL_COUNT) ||
        (count > (ACTUATOR_SERVICE_CHANNEL_COUNT - first_channel)))
    {
        return ACTUATOR_SERVICE_STATUS_BAD_ARG;
    }

    for (index = 0U; index < count; ++index)
    {
        status = PCA9685_PulseUsToCount(&pca9685,
                                        pulse_us[index],
                                        &pwm_count);
        if (status != PCA9685_STATUS_OK)
        {
            return ActuatorService_MapPCA9685Status(status);
        }
    }

    status = PCA9685_SetChannelsPulseUs(&pca9685,
                                        first_channel,
                                        pulse_us,
                                        count);
    if (status == PCA9685_STATUS_OK)
    {
        memcpy(&actuator_pulse_us[first_channel],
               pulse_us,
               (size_t)count * sizeof(pulse_us[0]));
    }
    return ActuatorService_MapPCA9685Status(status);
}

static Servo_StatusTypeDef ActuatorService_SubmitServoPulseUs(
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count)
{
    ActuatorService_StatusTypeDef status;

    if ((first_channel > ACTUATOR_SERVICE_SERVO_CHANNEL_LAST) ||
        (count == 0U) ||
        (count > ((ACTUATOR_SERVICE_SERVO_CHANNEL_LAST + 1U) - first_channel)))
    {
        return SERVO_STATUS_BAD_ARG;
    }

    status = ActuatorService_ApplyPulseRange(first_channel, pulse_us, count);
    switch (status)
    {
        case ACTUATOR_SERVICE_STATUS_OK:
            return SERVO_STATUS_OK;
        case ACTUATOR_SERVICE_STATUS_BAD_ARG:
            return SERVO_STATUS_BAD_ARG;
        case ACTUATOR_SERVICE_STATUS_NOT_READY:
            return SERVO_STATUS_NOT_READY;
        default:
            return SERVO_STATUS_IO_ERROR;
    }
}

ActuatorService_StatusTypeDef ActuatorService_Init(I2C_HandleTypeDef *i2c)
{
    uint8_t channel;
    PCA9685_StatusTypeDef pca_status;
    Servo_StatusTypeDef servo_status;

    if (i2c == NULL)
    {
        return ACTUATOR_SERVICE_STATUS_BAD_ARG;
    }

    /* CH0..CH9 = servo midpoint; CH10..CH15 = ESC neutral. */
    for (channel = 0U; channel < ACTUATOR_SERVICE_CHANNEL_COUNT; ++channel)
    {
        actuator_pulse_us[channel] = ACTUATOR_SERVICE_SAFE_PULSE_US;
    }

    pca_status = PCA9685_Init(&pca9685, i2c, actuator_pulse_us);
    if (pca_status != PCA9685_STATUS_OK)
    {
        return ActuatorService_MapPCA9685Status(pca_status);
    }

    servo_status = servo_init(ActuatorService_SubmitServoPulseUs);
    return ActuatorService_MapServoStatus(servo_status);
}

ActuatorService_StatusTypeDef ActuatorService_SetServo(uint8_t id,
                                                       uint8_t angle)
{
    return ActuatorService_MapServoStatus(servo_set(id, angle));
}

ActuatorService_StatusTypeDef ActuatorService_SetServoAll(uint8_t angle)
{
    return ActuatorService_MapServoStatus(servo_set_all(angle));
}

ActuatorService_StatusTypeDef ActuatorService_GetServo(uint8_t id,
                                                       uint8_t *angle)
{
    return ActuatorService_MapServoStatus(servo_get(id, angle));
}

ActuatorService_StatusTypeDef ActuatorService_GetServoAll(
    uint8_t angles[ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT])
{
    return ActuatorService_MapServoStatus(servo_get_all(angles));
}

ActuatorService_StatusTypeDef ActuatorService_SetServoMid(uint8_t id)
{
    return ActuatorService_MapServoStatus(servo_mid(id));
}

ActuatorService_StatusTypeDef ActuatorService_SetServoAllMid(void)
{
    return ActuatorService_MapServoStatus(servo_mid_all());
}

ActuatorService_StatusTypeDef ActuatorService_SetChannelPulseUs(
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count)
{
    return ActuatorService_ApplyPulseRange(first_channel, pulse_us, count);
}

uint32_t ActuatorService_GetLastHalError(void)
{
    return PCA9685_GetLastHalError(&pca9685);
}
