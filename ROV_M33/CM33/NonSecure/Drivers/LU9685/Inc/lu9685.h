/**
  ******************************************************************************
  * @file    lu9685.h
  * @brief   LU9685-20CU I2C protocol driver.
  ******************************************************************************
  */

#ifndef LU9685_H
#define LU9685_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32mp2xx_hal.h"

#define LU9685_I2C_ADDRESS             0x00U
#define LU9685_CHANNEL_COUNT           20U
#define LU9685_MIN_ANGLE               0U
#define LU9685_MAX_ANGLE               180U
#define LU9685_OUTPUT_DISABLED         0xFFU

typedef enum
{
    LU9685_STATUS_OK = 0,
    LU9685_STATUS_BAD_ARG,
    LU9685_STATUS_NOT_READY,
    LU9685_STATUS_IO_ERROR
} LU9685_StatusTypeDef;

typedef struct
{
    I2C_HandleTypeDef *i2c;
    uint32_t last_hal_error;
    uint8_t initialized;
} LU9685_HandleTypeDef;

LU9685_StatusTypeDef LU9685_Init(LU9685_HandleTypeDef *device,
                                 I2C_HandleTypeDef *i2c);
LU9685_StatusTypeDef LU9685_Reset(LU9685_HandleTypeDef *device);
LU9685_StatusTypeDef LU9685_SetChannel(LU9685_HandleTypeDef *device,
                                      uint8_t channel,
                                      uint8_t angle);
LU9685_StatusTypeDef LU9685_CommitAll(
    LU9685_HandleTypeDef *device,
    const uint8_t channel_angle[LU9685_CHANNEL_COUNT]);
uint32_t LU9685_GetLastHalError(const LU9685_HandleTypeDef *device);

#ifdef __cplusplus
}
#endif

#endif /* LU9685_H */
