/**
  ******************************************************************************
  * @file    pca9685.c
  * @brief   PCA9685 16-channel, 12-bit PWM driver.
  ******************************************************************************
  */

#include "pca9685.h"

#include <stddef.h>

#define PCA9685_REG_MODE1              0x00U
#define PCA9685_REG_MODE2              0x01U
#define PCA9685_REG_LED0_ON_L          0x06U
#define PCA9685_REG_PRE_SCALE          0xFEU

#define PCA9685_MODE1_RESTART          0x80U
#define PCA9685_MODE1_AI               0x20U
#define PCA9685_MODE1_SLEEP            0x10U
#define PCA9685_MODE1_CLEAR_RESTART    0x7FU
#define PCA9685_MODE1_CLEAR_SLEEP      0xEFU
#define PCA9685_MODE2_OUTDRV           0x04U

#define PCA9685_I2C_TIMEOUT_MS          100U
#define PCA9685_OSCILLATOR_START_MS       1U
#define PCA9685_REGISTER_BYTES_PER_PWM    4U
#define PCA9685_COUNT_MAX              4095U
#define PCA9685_PRESCALE_MIN              3U

static uint16_t PCA9685_HalAddress(const PCA9685_HandleTypeDef *device)
{
    return (uint16_t)((uint16_t)device->address_7bit << 1U);
}

static PCA9685_StatusTypeDef PCA9685_WriteRegister(
    PCA9685_HandleTypeDef *device,
    uint8_t reg,
    const uint8_t *data,
    uint16_t size)
{
    HAL_StatusTypeDef status;

    if ((device == NULL) || (data == NULL) || (size == 0U))
    {
        return PCA9685_STATUS_BAD_ARG;
    }
    if (device->i2c == NULL)
    {
        return PCA9685_STATUS_NOT_READY;
    }

    status = HAL_I2C_Mem_Write(device->i2c,
                               PCA9685_HalAddress(device),
                               reg,
                               I2C_MEMADD_SIZE_8BIT,
                               (uint8_t *)data,
                               size,
                               PCA9685_I2C_TIMEOUT_MS);
    device->last_hal_error = HAL_I2C_GetError(device->i2c);
    return (status == HAL_OK) ? PCA9685_STATUS_OK
                              : PCA9685_STATUS_IO_ERROR;
}

static PCA9685_StatusTypeDef PCA9685_ReadByte(PCA9685_HandleTypeDef *device,
                                              uint8_t reg,
                                              uint8_t *value)
{
    HAL_StatusTypeDef status;

    if ((device == NULL) || (value == NULL))
    {
        return PCA9685_STATUS_BAD_ARG;
    }
    if (device->i2c == NULL)
    {
        return PCA9685_STATUS_NOT_READY;
    }

    status = HAL_I2C_Mem_Read(device->i2c,
                              PCA9685_HalAddress(device),
                              reg,
                              I2C_MEMADD_SIZE_8BIT,
                              value,
                              1U,
                              PCA9685_I2C_TIMEOUT_MS);
    device->last_hal_error = HAL_I2C_GetError(device->i2c);
    return (status == HAL_OK) ? PCA9685_STATUS_OK
                              : PCA9685_STATUS_IO_ERROR;
}

static PCA9685_StatusTypeDef PCA9685_CalculatePrescale(
    const PCA9685_HandleTypeDef *device,
    uint16_t frequency_hz,
    uint8_t *prescale)
{
    uint32_t divisor;
    uint32_t rounded_ratio;

    if ((device == NULL) || (prescale == NULL) ||
        (frequency_hz < PCA9685_MIN_PWM_FREQ_HZ) ||
        (frequency_hz > PCA9685_MAX_PWM_FREQ_HZ) ||
        (device->oscillator_hz == 0U))
    {
        return PCA9685_STATUS_BAD_ARG;
    }

    divisor = PCA9685_PWM_RESOLUTION * (uint32_t)frequency_hz;
    rounded_ratio = (device->oscillator_hz + (divisor / 2U)) / divisor;
    if ((rounded_ratio <= PCA9685_PRESCALE_MIN) ||
        (rounded_ratio > 256U))
    {
        return PCA9685_STATUS_BAD_ARG;
    }

    *prescale = (uint8_t)(rounded_ratio - 1U);
    return PCA9685_STATUS_OK;
}

static PCA9685_StatusTypeDef PCA9685_WritePulseRange(
    PCA9685_HandleTypeDef *device,
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count)
{
    uint8_t data[PCA9685_CHANNEL_COUNT * PCA9685_REGISTER_BYTES_PER_PWM];
    uint8_t index;
    uint16_t off_count;
    PCA9685_StatusTypeDef status;

    if ((device == NULL) || (pulse_us == NULL) || (count == 0U) ||
        (first_channel >= PCA9685_CHANNEL_COUNT) ||
        (count > (PCA9685_CHANNEL_COUNT - first_channel)))
    {
        return PCA9685_STATUS_BAD_ARG;
    }

    for (index = 0U; index < count; ++index)
    {
        status = PCA9685_PulseUsToCount(device, pulse_us[index], &off_count);
        if (status != PCA9685_STATUS_OK)
        {
            return status;
        }

        data[(uint16_t)index * 4U] = 0U;
        data[((uint16_t)index * 4U) + 1U] = 0U;
        data[((uint16_t)index * 4U) + 2U] = (uint8_t)(off_count & 0xFFU);
        data[((uint16_t)index * 4U) + 3U] =
            (uint8_t)((off_count >> 8U) & 0x0FU);
    }

    return PCA9685_WriteRegister(
        device,
        (uint8_t)(PCA9685_REG_LED0_ON_L +
                  ((uint16_t)first_channel * 4U)),
        data,
        (uint16_t)count * PCA9685_REGISTER_BYTES_PER_PWM);
}

PCA9685_StatusTypeDef PCA9685_Init(
    PCA9685_HandleTypeDef *device,
    I2C_HandleTypeDef *i2c,
    const uint16_t initial_pulse_us[PCA9685_CHANNEL_COUNT])
{
    uint8_t mode1;
    uint8_t mode2 = PCA9685_MODE2_OUTDRV;
    uint8_t prescale;
    PCA9685_StatusTypeDef status;

    if ((device == NULL) || (i2c == NULL) || (initial_pulse_us == NULL))
    {
        return PCA9685_STATUS_BAD_ARG;
    }

    device->i2c = i2c;
    device->oscillator_hz = PCA9685_INTERNAL_OSCILLATOR_HZ;
    device->last_hal_error = HAL_I2C_ERROR_NONE;
    device->pwm_frequency_hz = PCA9685_DEFAULT_PWM_FREQ_HZ;
    device->address_7bit = PCA9685_I2C_ADDRESS_7BIT;
    device->prescale = 0U;
    device->initialized = 0U;

    status = PCA9685_CalculatePrescale(device,
                                       PCA9685_DEFAULT_PWM_FREQ_HZ,
                                       &prescale);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }

    /* PRE_SCALE is writable only while SLEEP=1. AI enables the 64-byte load. */
    mode1 = PCA9685_MODE1_AI | PCA9685_MODE1_SLEEP;
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE1, &mode1, 1U);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE2, &mode2, 1U);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }
    status = PCA9685_WriteRegister(device,
                                    PCA9685_REG_PRE_SCALE,
                                    &prescale,
                                    1U);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }

    device->prescale = prescale;
    device->initialized = 1U;

    /* Program all application-safe pulses before the oscillator is started. */
    status = PCA9685_WritePulseRange(device,
                                     0U,
                                     initial_pulse_us,
                                     PCA9685_CHANNEL_COUNT);
    if (status != PCA9685_STATUS_OK)
    {
        device->initialized = 0U;
        return status;
    }

    mode1 = PCA9685_MODE1_AI;
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE1, &mode1, 1U);
    if (status != PCA9685_STATUS_OK)
    {
        device->initialized = 0U;
        return status;
    }
    HAL_Delay(PCA9685_OSCILLATOR_START_MS);

    mode1 = PCA9685_MODE1_RESTART | PCA9685_MODE1_AI;
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE1, &mode1, 1U);
    if (status != PCA9685_STATUS_OK)
    {
        device->initialized = 0U;
    }
    return status;
}

PCA9685_StatusTypeDef PCA9685_SetPWMFreq(PCA9685_HandleTypeDef *device,
                                         uint16_t frequency_hz)
{
    uint8_t old_mode;
    uint8_t sleep_mode;
    uint8_t wake_mode;
    uint8_t prescale;
    PCA9685_StatusTypeDef status;

    if ((device == NULL) || (device->initialized == 0U))
    {
        return PCA9685_STATUS_NOT_READY;
    }
    status = PCA9685_CalculatePrescale(device, frequency_hz, &prescale);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }
    status = PCA9685_ReadByte(device, PCA9685_REG_MODE1, &old_mode);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }

    sleep_mode = (uint8_t)((old_mode & PCA9685_MODE1_CLEAR_RESTART) |
                           PCA9685_MODE1_SLEEP | PCA9685_MODE1_AI);
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE1, &sleep_mode, 1U);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }
    status = PCA9685_WriteRegister(device,
                                    PCA9685_REG_PRE_SCALE,
                                    &prescale,
                                    1U);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }

    wake_mode = (uint8_t)(sleep_mode & PCA9685_MODE1_CLEAR_SLEEP);
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE1, &wake_mode, 1U);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }
    HAL_Delay(PCA9685_OSCILLATOR_START_MS);
    wake_mode |= PCA9685_MODE1_RESTART;
    status = PCA9685_WriteRegister(device, PCA9685_REG_MODE1, &wake_mode, 1U);
    if (status == PCA9685_STATUS_OK)
    {
        device->prescale = prescale;
        device->pwm_frequency_hz = frequency_hz;
    }
    return status;
}

PCA9685_StatusTypeDef PCA9685_SetPWM(PCA9685_HandleTypeDef *device,
                                     uint8_t channel,
                                     uint16_t on_count,
                                     uint16_t off_count)
{
    uint8_t data[4];

    if ((device == NULL) || (channel >= PCA9685_CHANNEL_COUNT) ||
        (on_count > PCA9685_COUNT_MAX) || (off_count > PCA9685_COUNT_MAX) ||
        (on_count == off_count))
    {
        return PCA9685_STATUS_BAD_ARG;
    }
    if (device->initialized == 0U)
    {
        return PCA9685_STATUS_NOT_READY;
    }

    data[0] = (uint8_t)(on_count & 0xFFU);
    data[1] = (uint8_t)((on_count >> 8U) & 0x0FU);
    data[2] = (uint8_t)(off_count & 0xFFU);
    data[3] = (uint8_t)((off_count >> 8U) & 0x0FU);
    return PCA9685_WriteRegister(device,
                                 (uint8_t)(PCA9685_REG_LED0_ON_L +
                                           ((uint16_t)channel * 4U)),
                                 data,
                                 sizeof(data));
}

PCA9685_StatusTypeDef PCA9685_PulseUsToCount(
    const PCA9685_HandleTypeDef *device,
    uint16_t pulse_us,
    uint16_t *count)
{
    uint64_t numerator;
    uint32_t tick_divisor;
    uint64_t rounded_count;

    if ((device == NULL) || (count == NULL))
    {
        return PCA9685_STATUS_BAD_ARG;
    }
    if ((device->initialized == 0U) || (device->oscillator_hz == 0U))
    {
        return PCA9685_STATUS_NOT_READY;
    }

    tick_divisor = ((uint32_t)device->prescale + 1U) * 1000000UL;
    numerator = (uint64_t)pulse_us * (uint64_t)device->oscillator_hz;
    rounded_count = (numerator + (tick_divisor / 2U)) / tick_divisor;
    if ((rounded_count == 0U) || (rounded_count > PCA9685_COUNT_MAX))
    {
        return PCA9685_STATUS_BAD_ARG;
    }

    *count = (uint16_t)rounded_count;
    return PCA9685_STATUS_OK;
}

PCA9685_StatusTypeDef PCA9685_SetPulseUs(PCA9685_HandleTypeDef *device,
                                         uint8_t channel,
                                         uint16_t pulse_us)
{
    return PCA9685_SetChannelPulseUs(device, channel, pulse_us);
}

PCA9685_StatusTypeDef PCA9685_SetChannelPulseUs(
    PCA9685_HandleTypeDef *device,
    uint8_t channel,
    uint16_t pulse_us)
{
    uint16_t count;
    PCA9685_StatusTypeDef status;

    status = PCA9685_PulseUsToCount(device, pulse_us, &count);
    if (status != PCA9685_STATUS_OK)
    {
        return status;
    }
    return PCA9685_SetPWM(device, channel, 0U, count);
}

PCA9685_StatusTypeDef PCA9685_SetChannelsPulseUs(
    PCA9685_HandleTypeDef *device,
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count)
{
    if (device == NULL)
    {
        return PCA9685_STATUS_BAD_ARG;
    }
    if (device->initialized == 0U)
    {
        return PCA9685_STATUS_NOT_READY;
    }
    return PCA9685_WritePulseRange(device, first_channel, pulse_us, count);
}

PCA9685_StatusTypeDef PCA9685_SetAllChannelPulseUs(
    PCA9685_HandleTypeDef *device,
    const uint16_t pulse_us[PCA9685_CHANNEL_COUNT])
{
    return PCA9685_SetChannelsPulseUs(device,
                                      0U,
                                      pulse_us,
                                      PCA9685_CHANNEL_COUNT);
}

uint32_t PCA9685_GetLastHalError(const PCA9685_HandleTypeDef *device)
{
    return (device == NULL) ? HAL_I2C_ERROR_NONE : device->last_hal_error;
}
