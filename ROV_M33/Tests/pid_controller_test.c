#include "pid_controller.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static int close_to(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
    PID_ControllerTypeDef pid;
    PID_Controller_ConfigTypeDef config = {
        0.8f, 0.02f, 0.15f, 10.0f, -5.0f, 5.0f, -10.0f, 10.0f
    };
    float error;
    float output;
    unsigned int index;

    assert(PID_Controller_Init(&pid, &config) == PID_CONTROLLER_STATUS_OK);
    assert(PID_Controller_Update(&pid, 0.0f, 10.0f, 0.02f,
                                 &error, &output) == PID_CONTROLLER_STATUS_OK);
    assert(error == 0.0f && output == 0.0f);
    assert(PID_Controller_Update(&pid, 0.0f, 11.0f, 0.02f,
                                 &error, &output) == PID_CONTROLLER_STATUS_OK);
    assert(close_to(error, -1.0f, 0.001f));
    assert(close_to(output, -8.3004f, 0.001f)); /* D uses measurement delta. */
    assert(PID_Controller_Update(&pid, 0.0f, 11.0f, 0.02f,
                                 &error, &output) == PID_CONTROLLER_STATUS_OK);
    assert(output < -0.8f && output > -0.9f); /* P + I, no D kick. */

    config.kp = 0.0f;
    config.ki = 1.0f;
    config.kd = 0.0f;
    config.threshold = 0.0f;
    config.integral_min = -5.0f;
    config.integral_max = 5.0f;
    assert(PID_Controller_Init(&pid, &config) == PID_CONTROLLER_STATUS_OK);
    for (index = 0U; index < 20U; ++index)
    {
        assert(PID_Controller_Update(&pid, 10.0f, 0.0f, 1.0f,
                                     &error, &output) == PID_CONTROLLER_STATUS_OK);
    }
    assert(close_to(pid.integral_term, 5.0f, 0.001f));
    assert(close_to(output, 5.0f, 0.001f));

    config.kp = 10.0f;
    config.ki = 1.0f;
    config.output_min = -2.0f;
    config.output_max = 2.0f;
    assert(PID_Controller_Init(&pid, &config) == PID_CONTROLLER_STATUS_OK);
    assert(PID_Controller_Update(&pid, 10.0f, 0.0f, 1.0f,
                                 &error, &output) == PID_CONTROLLER_STATUS_OK);
    assert(output == 2.0f && pid.integral_term == 0.0f);
    PID_Controller_Reset(&pid);
    assert(pid.integral_term == 0.0f && pid.has_previous_measurement == 0U);
    puts("pid_controller_test: PASS");
    return 0;
}
