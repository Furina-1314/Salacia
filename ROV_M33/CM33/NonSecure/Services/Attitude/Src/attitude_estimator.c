/**
  ******************************************************************************
  * @file    attitude_estimator.c
  * @brief   Roll/pitch estimator using MPU6500 calibration and complementary filter.
  ******************************************************************************
  */

#include "attitude_estimator.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define ATTITUDE_ESTIMATOR_RAD_TO_DEG       57.2957795131f
#define ATTITUDE_ESTIMATOR_MIN_GRAVITY_SQ    1.0f

typedef struct
{
    AttitudeEstimator_ConfigTypeDef config;
    AttitudeEstimator_StateTypeDef state;
    int64_t accel_sum[3];
    int64_t gyro_sum[3];
    float reference_roll_deg;
    float reference_pitch_deg;
    uint8_t initialized;
} AttitudeEstimator_ContextTypeDef;

static AttitudeEstimator_ContextTypeDef attitude_estimator;

static float AttitudeEstimator_Wrap180(float angle)
{
    while (angle > 180.0f)
    {
        angle -= 360.0f;
    }
    while (angle < -180.0f)
    {
        angle += 360.0f;
    }
    return angle;
}

static void AttitudeEstimator_AccelAngles(float ax,
                                          float ay,
                                          float az,
                                          float *roll_deg,
                                          float *pitch_deg)
{
    *roll_deg = atan2f(ay, az) * ATTITUDE_ESTIMATOR_RAD_TO_DEG;
    *pitch_deg = atan2f(-ax, sqrtf((ay * ay) + (az * az))) *
                 ATTITUDE_ESTIMATOR_RAD_TO_DEG;
}

static AttitudeEstimator_StatusTypeDef AttitudeEstimator_FinishCalibration(void)
{
    float divisor = (float)attitude_estimator.config.calibration_samples;
    float gravity_sq;
    uint8_t axis;

    for (axis = 0U; axis < 3U; ++axis)
    {
        attitude_estimator.state.accel_reference[axis] =
            (float)attitude_estimator.accel_sum[axis] / divisor;
        attitude_estimator.state.gyro_bias[axis] =
            (float)attitude_estimator.gyro_sum[axis] / divisor;
    }

    gravity_sq =
        (attitude_estimator.state.accel_reference[0] *
         attitude_estimator.state.accel_reference[0]) +
        (attitude_estimator.state.accel_reference[1] *
         attitude_estimator.state.accel_reference[1]) +
        (attitude_estimator.state.accel_reference[2] *
         attitude_estimator.state.accel_reference[2]);
    if (gravity_sq < ATTITUDE_ESTIMATOR_MIN_GRAVITY_SQ)
    {
        return ATTITUDE_ESTIMATOR_STATUS_NOT_READY;
    }

    AttitudeEstimator_AccelAngles(
        attitude_estimator.state.accel_reference[0],
        attitude_estimator.state.accel_reference[1],
        attitude_estimator.state.accel_reference[2],
        &attitude_estimator.reference_roll_deg,
        &attitude_estimator.reference_pitch_deg);

    attitude_estimator.state.roll_deg = 0.0f;
    attitude_estimator.state.pitch_deg = 0.0f;
    attitude_estimator.state.roll_rate_dps = 0.0f;
    attitude_estimator.state.pitch_rate_dps = 0.0f;
    attitude_estimator.state.ready = 1U;
    return ATTITUDE_ESTIMATOR_STATUS_OK;
}

AttitudeEstimator_StatusTypeDef AttitudeEstimator_Init(
    const AttitudeEstimator_ConfigTypeDef *config)
{
    if ((config == NULL) || (config->alpha < 0.0f) ||
        (config->alpha > 1.0f) || (config->gyro_lsb_per_dps <= 0.0f) ||
        (config->calibration_samples == 0U))
    {
        return ATTITUDE_ESTIMATOR_STATUS_BAD_ARG;
    }

    memset(&attitude_estimator, 0, sizeof(attitude_estimator));
    attitude_estimator.config = *config;
    attitude_estimator.initialized = 1U;
    return ATTITUDE_ESTIMATOR_STATUS_OK;
}

void AttitudeEstimator_Reset(void)
{
    AttitudeEstimator_ConfigTypeDef config = attitude_estimator.config;
    uint8_t initialized = attitude_estimator.initialized;

    memset(&attitude_estimator, 0, sizeof(attitude_estimator));
    attitude_estimator.config = config;
    attitude_estimator.initialized = initialized;
}

AttitudeEstimator_StatusTypeDef AttitudeEstimator_Update(
    const MPU6500_RawData *sample,
    float dt_seconds)
{
    float accel_roll_deg;
    float accel_pitch_deg;
    float gyro_roll_deg;
    float gyro_pitch_deg;

    if ((sample == NULL) || (attitude_estimator.initialized == 0U))
    {
        return ATTITUDE_ESTIMATOR_STATUS_BAD_ARG;
    }

    if (attitude_estimator.state.ready == 0U)
    {
        attitude_estimator.accel_sum[0] += sample->ax;
        attitude_estimator.accel_sum[1] += sample->ay;
        attitude_estimator.accel_sum[2] += sample->az;
        attitude_estimator.gyro_sum[0] += sample->gx;
        attitude_estimator.gyro_sum[1] += sample->gy;
        attitude_estimator.gyro_sum[2] += sample->gz;
        ++attitude_estimator.state.calibration_count;

        if (attitude_estimator.state.calibration_count <
            attitude_estimator.config.calibration_samples)
        {
            return ATTITUDE_ESTIMATOR_STATUS_CALIBRATING;
        }
        return AttitudeEstimator_FinishCalibration();
    }

    if ((dt_seconds <= 0.0f) || (dt_seconds > 0.05f))
    {
        return ATTITUDE_ESTIMATOR_STATUS_BAD_ARG;
    }

    AttitudeEstimator_AccelAngles((float)sample->ax,
                                  (float)sample->ay,
                                  (float)sample->az,
                                  &accel_roll_deg,
                                  &accel_pitch_deg);
    accel_roll_deg = AttitudeEstimator_Wrap180(
        accel_roll_deg - attitude_estimator.reference_roll_deg);
    accel_pitch_deg = AttitudeEstimator_Wrap180(
        accel_pitch_deg - attitude_estimator.reference_pitch_deg);

    attitude_estimator.state.roll_rate_dps =
        ((float)sample->gx - attitude_estimator.state.gyro_bias[0]) /
        attitude_estimator.config.gyro_lsb_per_dps;
    attitude_estimator.state.pitch_rate_dps =
        ((float)sample->gy - attitude_estimator.state.gyro_bias[1]) /
        attitude_estimator.config.gyro_lsb_per_dps;

    gyro_roll_deg = attitude_estimator.state.roll_deg +
        (attitude_estimator.state.roll_rate_dps * dt_seconds);
    gyro_pitch_deg = attitude_estimator.state.pitch_deg +
        (attitude_estimator.state.pitch_rate_dps * dt_seconds);

    attitude_estimator.state.roll_deg =
        (attitude_estimator.config.alpha * gyro_roll_deg) +
        ((1.0f - attitude_estimator.config.alpha) * accel_roll_deg);
    attitude_estimator.state.pitch_deg =
        (attitude_estimator.config.alpha * gyro_pitch_deg) +
        ((1.0f - attitude_estimator.config.alpha) * accel_pitch_deg);

    return ATTITUDE_ESTIMATOR_STATUS_OK;
}

uint8_t AttitudeEstimator_IsReady(void)
{
    return attitude_estimator.state.ready;
}

AttitudeEstimator_StatusTypeDef AttitudeEstimator_GetState(
    AttitudeEstimator_StateTypeDef *state)
{
    if (state == NULL)
    {
        return ATTITUDE_ESTIMATOR_STATUS_BAD_ARG;
    }
    if (attitude_estimator.initialized == 0U)
    {
        return ATTITUDE_ESTIMATOR_STATUS_NOT_READY;
    }
    *state = attitude_estimator.state;
    return ATTITUDE_ESTIMATOR_STATUS_OK;
}
