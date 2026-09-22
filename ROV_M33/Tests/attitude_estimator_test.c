#include "attitude_estimator.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static int close_to(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
    AttitudeEstimator_ConfigTypeDef config = {0.98f, 131.0f, 200U};
    AttitudeEstimator_StateTypeDef state;
    MPU6500_RawData sample = {0, 0, 16384, 131, -262, 0};
    uint16_t index;

    assert(AttitudeEstimator_Init(&config) == ATTITUDE_ESTIMATOR_STATUS_OK);
    for (index = 0U; index < 199U; ++index)
    {
        assert(AttitudeEstimator_Update(&sample, 0.01f) ==
               ATTITUDE_ESTIMATOR_STATUS_CALIBRATING);
    }
    assert(AttitudeEstimator_IsReady() == 0U);
    assert(AttitudeEstimator_Update(&sample, 0.01f) ==
           ATTITUDE_ESTIMATOR_STATUS_OK);
    assert(AttitudeEstimator_IsReady() == 1U);
    assert(AttitudeEstimator_GetState(&state) == ATTITUDE_ESTIMATOR_STATUS_OK);
    assert(close_to(state.gyro_bias[0], 131.0f, 0.01f));
    assert(close_to(state.gyro_bias[1], -262.0f, 0.01f));
    assert(close_to(state.roll_deg, 0.0f, 0.01f));

    sample.ay = 8192;
    sample.az = 14189;
    assert(AttitudeEstimator_Update(&sample, 0.01f) ==
           ATTITUDE_ESTIMATOR_STATUS_OK);
    assert(AttitudeEstimator_GetState(&state) == ATTITUDE_ESTIMATOR_STATUS_OK);
    assert(state.roll_deg > 0.5f && state.roll_deg < 0.7f);
    assert(AttitudeEstimator_Update(&sample, 0.1f) ==
           ATTITUDE_ESTIMATOR_STATUS_BAD_ARG);

    AttitudeEstimator_Reset();
    assert(AttitudeEstimator_IsReady() == 0U);
    puts("attitude_estimator_test: PASS");
    return 0;
}
