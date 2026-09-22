/**
  ******************************************************************************
  * @file    mpu6500.h
  * @brief   Blocking I2C driver for MPU6500 raw sensor data.
  ******************************************************************************
  */

#ifndef MPU6500_H
#define MPU6500_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32mp2xx_hal.h"

#define MPU6500_I2C_ADDRESS_7BIT       0x68U
#define MPU6500_I2C_ADDRESS_HAL         (MPU6500_I2C_ADDRESS_7BIT << 1)
#define MPU6500_WHO_AM_I_EXPECTED       0x70U

typedef enum
{
    MPU6500_STATUS_OK = 0,
    MPU6500_STATUS_BAD_ARG,
    MPU6500_STATUS_NOT_READY,
    MPU6500_STATUS_BUSY,
    MPU6500_STATUS_TIMEOUT,
    MPU6500_STATUS_IO_ERROR
} MPU6500_StatusTypeDef;

typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} MPU6500_RawData;

typedef struct
{
    I2C_HandleTypeDef *i2c;
    uint32_t last_hal_error;
    HAL_StatusTypeDef last_hal_status;
    uint8_t who_am_i;
    uint8_t initialized;
} MPU6500_HandleTypeDef;

MPU6500_StatusTypeDef MPU6500_Init(MPU6500_HandleTypeDef *device,
                                   I2C_HandleTypeDef *i2c);
MPU6500_StatusTypeDef MPU6500_ReadRaw(MPU6500_HandleTypeDef *device,
                                      MPU6500_RawData *data);
uint8_t MPU6500_GetWhoAmI(const MPU6500_HandleTypeDef *device);
uint32_t MPU6500_GetLastHalError(const MPU6500_HandleTypeDef *device);

#ifdef __cplusplus
}
#endif

#endif /* MPU6500_H */
