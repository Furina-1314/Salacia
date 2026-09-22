#include "mpu6500.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t last_read_reg;
static uint16_t last_read_size;
static uint32_t last_read_timeout;

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *i2c,
                                        uint16_t address,
                                        uint32_t trials,
                                        uint32_t timeout)
{
    (void)i2c; (void)address; (void)trials; (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *i2c,
                                    uint16_t address, uint16_t reg,
                                    uint16_t mem_address_size, uint8_t *data,
                                    uint16_t size, uint32_t timeout)
{
    (void)i2c; (void)address; (void)reg; (void)mem_address_size;
    (void)data; (void)size; (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *i2c,
                                   uint16_t address, uint16_t reg,
                                   uint16_t mem_address_size, uint8_t *data,
                                   uint16_t size, uint32_t timeout)
{
    (void)i2c; (void)address; (void)mem_address_size;
    last_read_reg = reg;
    last_read_size = size;
    last_read_timeout = timeout;
    memset(data, 0, size);
    if (reg == 0x75U)
    {
        data[0] = MPU6500_WHO_AM_I_EXPECTED;
    }
    else if ((reg == 0x3BU) && (size == 14U))
    {
        data[0] = 0x01U; data[1] = 0x02U;
        data[2] = 0x03U; data[3] = 0x04U;
        data[4] = 0x05U; data[5] = 0x06U;
        data[8] = 0x07U; data[9] = 0x08U;
        data[10] = 0x09U; data[11] = 0x0AU;
        data[12] = 0x0BU; data[13] = 0x0CU;
    }
    return HAL_OK;
}

uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *i2c)
{
    (void)i2c;
    return 0U;
}

void HAL_Delay(uint32_t delay_ms)
{
    (void)delay_ms;
}

int main(void)
{
    I2C_HandleTypeDef i2c;
    MPU6500_HandleTypeDef device;
    MPU6500_RawData data;

    assert(MPU6500_Init(&device, &i2c) == MPU6500_STATUS_OK);
    assert(MPU6500_ReadRaw(&device, &data) == MPU6500_STATUS_OK);
    assert(last_read_reg == 0x3BU && last_read_size == 14U);
    assert(last_read_timeout == 10U);
    assert(data.ax == 0x0102 && data.ay == 0x0304 && data.az == 0x0506);
    assert(data.gx == 0x0708 && data.gy == 0x090A && data.gz == 0x0B0C);
    puts("mpu6500_test: PASS");
    return 0;
}
