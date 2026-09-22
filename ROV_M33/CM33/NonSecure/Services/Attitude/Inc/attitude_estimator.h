/**
  ******************************************************************************
  * @file    attitude_estimator.h
  * @brief   Roll/pitch estimator using MPU6500 calibration and complementary filter.
  ******************************************************************************
  */

#ifndef ATTITUDE_ESTIMATOR_H
#define ATTITUDE_ESTIMATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "mpu6500.h"

#define ATTITUDE_ESTIMATOR_DEFAULT_ALPHA                0.98f
#define ATTITUDE_ESTIMATOR_DEFAULT_CALIBRATION_SAMPLES  200U
#define ATTITUDE_ESTIMATOR_GYRO_LSB_PER_DPS             131.0f

typedef enum
{
    ATTITUDE_ESTIMATOR_STATUS_OK = 0,
    ATTITUDE_ESTIMATOR_STATUS_CALIBRATING,
    ATTITUDE_ESTIMATOR_STATUS_BAD_ARG,
    ATTITUDE_ESTIMATOR_STATUS_NOT_READY
} AttitudeEstimator_StatusTypeDef;

typedef struct
{
    float alpha;
    float gyro_lsb_per_dps;
    uint16_t calibration_samples;
} AttitudeEstimator_ConfigTypeDef;

typedef struct
{
    float gyro_bias[3];
    float accel_reference[3];
    float roll_deg;
    float pitch_deg;
    float roll_rate_dps;
    float pitch_rate_dps;
    uint16_t calibration_count;
    uint8_t ready;
} AttitudeEstimator_StateTypeDef;

AttitudeEstimator_StatusTypeDef AttitudeEstimator_Init(
    const AttitudeEstimator_ConfigTypeDef *config);
void AttitudeEstimator_Reset(void);
AttitudeEstimator_StatusTypeDef AttitudeEstimator_Update(
    const MPU6500_RawData *sample,
    float dt_seconds);
uint8_t AttitudeEstimator_IsReady(void);
AttitudeEstimator_StatusTypeDef AttitudeEstimator_GetState(
    AttitudeEstimator_StateTypeDef *state);

#ifdef __cplusplus
}
#endif

#endif /* ATTITUDE_ESTIMATOR_H */
