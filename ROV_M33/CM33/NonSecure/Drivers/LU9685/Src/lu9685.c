/**
  ******************************************************************************
  * @file    lu9685.c
  * @brief   LU9685-20CU I2C protocol driver.
  ******************************************************************************
  */

#include "lu9685.h"

#include <string.h>

#define LU9685_RESET_BYTE              0xFBU
#define LU9685_COMMIT_ALL_BYTE         0xFDU
#define LU9685_I2C_TIMEOUT_MS          100U

static LU9685_StatusTypeDef LU9685_Transmit(LU9685_HandleTypeDef *device,
                                            uint8_t *data,
                                            uint16_t size)
{
    HAL_StatusTypeDef status;

    if ((device == NULL) || (data == NULL) || (size == 0U))
    {
        return LU9685_STATUS_BAD_ARG;
    }

    if ((device->initialized == 0U) || (device->i2c == NULL))
    {
        return LU9685_STATUS_NOT_READY;
    }

    status = HAL_I2C_Master_Transmit(device->i2c,
                                     LU9685_I2C_ADDRESS,
                                     data,
                                     size,
                                     LU9685_I2C_TIMEOUT_MS);
    device->last_hal_error = HAL_I2C_GetError(device->i2c);

    return (status == HAL_OK) ? LU9685_STATUS_OK : LU9685_STATUS_IO_ERROR;
}

LU9685_StatusTypeDef LU9685_Init(LU9685_HandleTypeDef *device,
                                 I2C_HandleTypeDef *i2c)
{
    if ((device == NULL) || (i2c == NULL))
    {
        return LU9685_STATUS_BAD_ARG;
    }

    device->i2c = i2c;
    device->last_hal_error = HAL_I2C_ERROR_NONE;
    device->initialized = 1U;

    return LU9685_STATUS_OK;
}

LU9685_StatusTypeDef LU9685_Reset(LU9685_HandleTypeDef *device)
{
    uint8_t command[2] = {LU9685_RESET_BYTE, LU9685_RESET_BYTE};

    return LU9685_Transmit(device, command, sizeof(command));
}

LU9685_StatusTypeDef LU9685_SetChannel(LU9685_HandleTypeDef *device,
                                      uint8_t channel,
                                      uint8_t angle)
{
    uint8_t command[2];

    if ((channel >= LU9685_CHANNEL_COUNT) || (angle > LU9685_MAX_ANGLE))
    {
        return LU9685_STATUS_BAD_ARG;
    }

    command[0] = channel;
    command[1] = angle;

    return LU9685_Transmit(device, command, sizeof(command));
}

LU9685_StatusTypeDef LU9685_CommitAll(
    LU9685_HandleTypeDef *device,
    const uint8_t channel_angle[LU9685_CHANNEL_COUNT])
{
    uint8_t command[LU9685_CHANNEL_COUNT + 1U];

    if (channel_angle == NULL)
    {
        return LU9685_STATUS_BAD_ARG;
    }

    command[0] = LU9685_COMMIT_ALL_BYTE;
    memcpy(&command[1], channel_angle, LU9685_CHANNEL_COUNT);

    return LU9685_Transmit(device, command, sizeof(command));
}

uint32_t LU9685_GetLastHalError(const LU9685_HandleTypeDef *device)
{
    return (device == NULL) ? HAL_I2C_ERROR_NONE : device->last_hal_error;
}
