/**
  ******************************************************************************
  * @file    mpu6500.c
  * @brief   Blocking I2C driver for MPU6500 raw sensor data.
  ******************************************************************************
  */

#include "mpu6500.h"

#define MPU6500_REG_SMPLRT_DIV          0x19U
#define MPU6500_REG_CONFIG              0x1AU
#define MPU6500_REG_GYRO_CONFIG         0x1BU
#define MPU6500_REG_ACCEL_CONFIG        0x1CU
#define MPU6500_REG_ACCEL_OUT           0x3BU
#define MPU6500_REG_PWR_MGMT_1           0x6BU
#define MPU6500_REG_WHO_AM_I             0x75U

#define MPU6500_INIT_TIMEOUT_MS          500U
#define MPU6500_RUNTIME_TIMEOUT_MS        10U
#define MPU6500_PROBE_TRIALS             3U
#define MPU6500_WAKE_DELAY_MS            100U

static MPU6500_StatusTypeDef MPU6500_MapHalStatus(
    MPU6500_HandleTypeDef *device,
    HAL_StatusTypeDef hal_status)
{
    if (device != NULL)
    {
        device->last_hal_status = hal_status;
        device->last_hal_error = (device->i2c != NULL) ?
                                 HAL_I2C_GetError(device->i2c) : 0U;
    }

    if (hal_status == HAL_OK)
    {
        return MPU6500_STATUS_OK;
    }
    if (hal_status == HAL_BUSY)
    {
        return MPU6500_STATUS_BUSY;
    }
    if (hal_status == HAL_TIMEOUT)
    {
        return MPU6500_STATUS_TIMEOUT;
    }

    return MPU6500_STATUS_IO_ERROR;
}

static MPU6500_StatusTypeDef MPU6500_WriteByte(
    MPU6500_HandleTypeDef *device,
    uint8_t reg,
    uint8_t value)
{
    HAL_StatusTypeDef hal_status;

    hal_status = HAL_I2C_Mem_Write(device->i2c,
                                   MPU6500_I2C_ADDRESS_HAL,
                                   reg,
                                   I2C_MEMADD_SIZE_8BIT,
                                   &value,
                                   1U,
                                   MPU6500_INIT_TIMEOUT_MS);

    return MPU6500_MapHalStatus(device, hal_status);
}

static MPU6500_StatusTypeDef MPU6500_ReadBytes(
    MPU6500_HandleTypeDef *device,
    uint8_t reg,
    uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_status;

    hal_status = HAL_I2C_Mem_Read(device->i2c,
                                  MPU6500_I2C_ADDRESS_HAL,
                                  reg,
                                  I2C_MEMADD_SIZE_8BIT,
                                  data,
                                  length,
                                  timeout_ms);

    return MPU6500_MapHalStatus(device, hal_status);
}

static int16_t MPU6500_CombineBytes(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << 8) | (uint16_t)low);
}

MPU6500_StatusTypeDef MPU6500_Init(MPU6500_HandleTypeDef *device,
                                   I2C_HandleTypeDef *i2c)
{
    HAL_StatusTypeDef hal_status;
    MPU6500_StatusTypeDef status;

    if ((device == NULL) || (i2c == NULL))
    {
        return MPU6500_STATUS_BAD_ARG;
    }

    device->i2c = i2c;
    device->last_hal_error = 0U;
    device->last_hal_status = HAL_OK;
    device->who_am_i = 0U;
    device->initialized = 0U;

    hal_status = HAL_I2C_IsDeviceReady(device->i2c,
                                       MPU6500_I2C_ADDRESS_HAL,
                                       MPU6500_PROBE_TRIALS,
                                   MPU6500_INIT_TIMEOUT_MS);
    status = MPU6500_MapHalStatus(device, hal_status);
    if (status != MPU6500_STATUS_OK)
    {
        return (status == MPU6500_STATUS_IO_ERROR) ?
               MPU6500_STATUS_NOT_READY : status;
    }

    status = MPU6500_ReadBytes(device,
                               MPU6500_REG_WHO_AM_I,
                               &device->who_am_i,
                               1U,
                               MPU6500_INIT_TIMEOUT_MS);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    if (device->who_am_i != MPU6500_WHO_AM_I_EXPECTED)
    {
        return MPU6500_STATUS_NOT_READY;
    }

    status = MPU6500_WriteByte(device,
                               MPU6500_REG_PWR_MGMT_1,
                               0x00U);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    HAL_Delay(MPU6500_WAKE_DELAY_MS);

    status = MPU6500_WriteByte(device,
                               MPU6500_REG_SMPLRT_DIV,
                               0x07U);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    status = MPU6500_WriteByte(device,
                               MPU6500_REG_CONFIG,
                               0x00U);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    status = MPU6500_WriteByte(device,
                               MPU6500_REG_GYRO_CONFIG,
                               0x00U);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    status = MPU6500_WriteByte(device,
                               MPU6500_REG_ACCEL_CONFIG,
                               0x00U);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    device->initialized = 1U;
    return MPU6500_STATUS_OK;
}

MPU6500_StatusTypeDef MPU6500_ReadRaw(MPU6500_HandleTypeDef *device,
                                      MPU6500_RawData *data)
{
    MPU6500_StatusTypeDef status;
    uint8_t burst[14];
    MPU6500_RawData sample;

    if ((device == NULL) || (data == NULL))
    {
        return MPU6500_STATUS_BAD_ARG;
    }

    if ((device->initialized == 0U) || (device->i2c == NULL))
    {
        return MPU6500_STATUS_NOT_READY;
    }

    status = MPU6500_ReadBytes(device,
                               MPU6500_REG_ACCEL_OUT,
                               burst,
                               sizeof(burst),
                               MPU6500_RUNTIME_TIMEOUT_MS);
    if (status != MPU6500_STATUS_OK)
    {
        return status;
    }

    sample.ax = MPU6500_CombineBytes(burst[0], burst[1]);
    sample.ay = MPU6500_CombineBytes(burst[2], burst[3]);
    sample.az = MPU6500_CombineBytes(burst[4], burst[5]);
    sample.gx = MPU6500_CombineBytes(burst[8], burst[9]);
    sample.gy = MPU6500_CombineBytes(burst[10], burst[11]);
    sample.gz = MPU6500_CombineBytes(burst[12], burst[13]);

    *data = sample;
    return MPU6500_STATUS_OK;
}

uint8_t MPU6500_GetWhoAmI(const MPU6500_HandleTypeDef *device)
{
    return (device != NULL) ? device->who_am_i : 0U;
}

uint32_t MPU6500_GetLastHalError(const MPU6500_HandleTypeDef *device)
{
    return (device != NULL) ? device->last_hal_error : 0U;
}
