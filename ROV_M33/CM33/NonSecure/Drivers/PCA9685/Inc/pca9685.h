/**
  ******************************************************************************
  * @file    pca9685.h
  * @brief   PCA9685 16-channel, 12-bit PWM driver.
  ******************************************************************************
  */

#ifndef PCA9685_H
#define PCA9685_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32mp2xx_hal.h"

#define PCA9685_I2C_ADDRESS_7BIT       0x40U
#define PCA9685_CHANNEL_COUNT            16U
#define PCA9685_PWM_RESOLUTION         4096U
/* The datasheet nominal clock is 25 MHz, but the internal oscillator of this
 * part runs ~10% fast: with 25 MHz the commanded 1500 us ESC neutral measured
 * ~1360 us and the thrusters only spun up above ~+30% command (2026-09-15
 * bench observation). Calibrated value: ESC start threshold c satisfies
 * 1500 = (1500 + 5*c) * 25e6 / f_real with c = 30 -> f_real = 27.5 MHz.
 * Re-tune after deployment: if thrusters still start at +x%, set
 * f = 27.5e6 * (1500 + 5*x) / 1500. */
#define PCA9685_INTERNAL_OSCILLATOR_HZ 27500000UL
#define PCA9685_DEFAULT_PWM_FREQ_HZ       50U
#define PCA9685_MIN_PWM_FREQ_HZ           40U
#define PCA9685_MAX_PWM_FREQ_HZ         1000U

typedef enum
{
    PCA9685_STATUS_OK = 0,
    PCA9685_STATUS_BAD_ARG,
    PCA9685_STATUS_NOT_READY,
    PCA9685_STATUS_IO_ERROR
} PCA9685_StatusTypeDef;

typedef struct
{
    I2C_HandleTypeDef *i2c;
    uint32_t oscillator_hz;
    uint32_t last_hal_error;
    uint16_t pwm_frequency_hz;
    uint8_t address_7bit;
    uint8_t prescale;
    uint8_t initialized;
} PCA9685_HandleTypeDef;

PCA9685_StatusTypeDef PCA9685_Init(
    PCA9685_HandleTypeDef *device,
    I2C_HandleTypeDef *i2c,
    const uint16_t initial_pulse_us[PCA9685_CHANNEL_COUNT]);
PCA9685_StatusTypeDef PCA9685_SetPWMFreq(PCA9685_HandleTypeDef *device,
                                         uint16_t frequency_hz);
PCA9685_StatusTypeDef PCA9685_SetPWM(PCA9685_HandleTypeDef *device,
                                     uint8_t channel,
                                     uint16_t on_count,
                                     uint16_t off_count);
PCA9685_StatusTypeDef PCA9685_SetPulseUs(PCA9685_HandleTypeDef *device,
                                         uint8_t channel,
                                         uint16_t pulse_us);
PCA9685_StatusTypeDef PCA9685_SetChannelPulseUs(
    PCA9685_HandleTypeDef *device,
    uint8_t channel,
    uint16_t pulse_us);
PCA9685_StatusTypeDef PCA9685_SetChannelsPulseUs(
    PCA9685_HandleTypeDef *device,
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count);
PCA9685_StatusTypeDef PCA9685_SetAllChannelPulseUs(
    PCA9685_HandleTypeDef *device,
    const uint16_t pulse_us[PCA9685_CHANNEL_COUNT]);
PCA9685_StatusTypeDef PCA9685_PulseUsToCount(
    const PCA9685_HandleTypeDef *device,
    uint16_t pulse_us,
    uint16_t *count);
uint32_t PCA9685_GetLastHalError(const PCA9685_HandleTypeDef *device);

#ifdef __cplusplus
}
#endif

#endif /* PCA9685_H */
