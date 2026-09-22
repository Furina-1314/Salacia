#include "pca9685.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t address;
    uint16_t reg;
    uint16_t size;
    uint8_t data[64];
} WriteRecord;

static WriteRecord writes[8];
static uint8_t write_count;
static uint32_t delay_total_ms;

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *i2c,
                                    uint16_t address,
                                    uint16_t reg,
                                    uint16_t mem_address_size,
                                    uint8_t *data,
                                    uint16_t size,
                                    uint32_t timeout)
{
    WriteRecord *record;

    (void)i2c;
    (void)mem_address_size;
    (void)timeout;
    assert(write_count < (sizeof(writes) / sizeof(writes[0])));
    assert(size <= sizeof(writes[0].data));
    record = &writes[write_count++];
    record->address = address;
    record->reg = reg;
    record->size = size;
    memcpy(record->data, data, size);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *i2c,
                                   uint16_t address,
                                   uint16_t reg,
                                   uint16_t mem_address_size,
                                   uint8_t *data,
                                   uint16_t size,
                                   uint32_t timeout)
{
    (void)i2c;
    (void)address;
    (void)reg;
    (void)mem_address_size;
    (void)timeout;
    memset(data, 0, size);
    return HAL_OK;
}

uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *i2c)
{
    (void)i2c;
    return HAL_I2C_ERROR_NONE;
}

void HAL_Delay(uint32_t delay_ms)
{
    delay_total_ms += delay_ms;
}

static void test_initialization_sequence(void)
{
    I2C_HandleTypeDef i2c;
    PCA9685_HandleTypeDef device;
    uint16_t safe_pulses[PCA9685_CHANNEL_COUNT];
    uint8_t channel;

    memset(&i2c, 0, sizeof(i2c));
    memset(&device, 0, sizeof(device));
    memset(writes, 0, sizeof(writes));
    write_count = 0U;
    delay_total_ms = 0U;
    for (channel = 0U; channel < PCA9685_CHANNEL_COUNT; ++channel)
    {
        safe_pulses[channel] = 1500U;
    }

    assert(PCA9685_Init(&device, &i2c, safe_pulses) == PCA9685_STATUS_OK);
    assert(device.prescale == 121U);
    assert(device.pwm_frequency_hz == 50U);
    assert(write_count == 6U);
    assert(writes[0].reg == 0x00U && writes[0].data[0] == 0x30U);
    assert(writes[1].reg == 0x01U && writes[1].data[0] == 0x04U);
    assert(writes[2].reg == 0xFEU && writes[2].data[0] == 121U);
    assert(writes[3].reg == 0x06U && writes[3].size == 64U);
    assert(writes[3].data[0] == 0U && writes[3].data[1] == 0U);
    assert(writes[3].data[2] == 0x33U && writes[3].data[3] == 0x01U);
    assert(writes[3].data[42] == 0x33U && writes[3].data[43] == 0x01U);
    assert(writes[4].reg == 0x00U && writes[4].data[0] == 0x20U);
    assert(writes[5].reg == 0x00U && writes[5].data[0] == 0xA0U);
    assert(writes[0].address == 0x80U);
    assert(delay_total_ms == 1U);
}

static void test_count_mapping(void)
{
    PCA9685_HandleTypeDef device;
    uint16_t count;

    memset(&device, 0, sizeof(device));
    device.oscillator_hz = PCA9685_INTERNAL_OSCILLATOR_HZ;
    device.prescale = 121U;
    device.initialized = 1U;

    assert(PCA9685_PulseUsToCount(&device, 500U, &count) == PCA9685_STATUS_OK);
    assert(count == 102U);
    assert(PCA9685_PulseUsToCount(&device, 1000U, &count) == PCA9685_STATUS_OK);
    assert(count == 205U);
    assert(PCA9685_PulseUsToCount(&device, 1500U, &count) == PCA9685_STATUS_OK);
    assert(count == 307U);
    assert(PCA9685_PulseUsToCount(&device, 2000U, &count) == PCA9685_STATUS_OK);
    assert(count == 410U);
    assert(PCA9685_PulseUsToCount(&device, 2500U, &count) == PCA9685_STATUS_OK);
    assert(count == 512U);
}

int main(void)
{
    test_initialization_sequence();
    test_count_mapping();
    puts("pca9685_test: PASS");
    return 0;
}
