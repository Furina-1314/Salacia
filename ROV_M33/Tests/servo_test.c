#include "servo.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t submitted_first;
static uint8_t submitted_count;
static uint16_t submitted_pulses[SERVO_CHANNEL_COUNT];
static Servo_StatusTypeDef next_status;

static Servo_StatusTypeDef submit_pulses(uint8_t first_channel,
                                         const uint16_t *pulse_us,
                                         uint8_t count)
{
    Servo_StatusTypeDef result = next_status;

    submitted_first = first_channel;
    submitted_count = count;
    memcpy(submitted_pulses,
           pulse_us,
           (size_t)count * sizeof(pulse_us[0]));
    next_status = SERVO_STATUS_OK;
    return result;
}

int main(void)
{
    uint8_t angle;
    uint16_t pulse_us;

    assert(servo_init(submit_pulses) == SERVO_STATUS_OK);
    assert(servo_angle_to_pulse_us(0U, &pulse_us) == SERVO_STATUS_OK);
    assert(pulse_us == 500U);
    assert(servo_angle_to_pulse_us(90U, &pulse_us) == SERVO_STATUS_OK);
    assert(pulse_us == 1500U);
    assert(servo_angle_to_pulse_us(180U, &pulse_us) == SERVO_STATUS_OK);
    assert(pulse_us == 2500U);

    assert(servo_set(3U, 180U) == SERVO_STATUS_OK);
    assert(submitted_first == 3U);
    assert(submitted_count == 1U);
    assert(submitted_pulses[0] == 2500U);
    assert(servo_get(3U, &angle) == SERVO_STATUS_OK);
    assert(angle == 180U);

    assert(servo_set_all(0U) == SERVO_STATUS_OK);
    assert(submitted_first == 0U);
    assert(submitted_count == SERVO_CHANNEL_COUNT);
    assert(submitted_pulses[0] == 500U);
    assert(submitted_pulses[9] == 500U);

    next_status = SERVO_STATUS_IO_ERROR;
    assert(servo_set(3U, 90U) == SERVO_STATUS_IO_ERROR);
    assert(servo_get(3U, &angle) == SERVO_STATUS_OK);
    assert(angle == 0U);

    puts("servo_test: PASS");
    return 0;
}
