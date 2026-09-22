/**
  ******************************************************************************
  * @file    lu9685_channel_state.c
  * @brief   Ownership-aware software state for all 20 LU9685 channels.
  ******************************************************************************
  */

#include "lu9685_channel_state.h"

#include <string.h>

#define LU9685_SERVO_INITIAL_ANGLE       90U

/*
 * CH0..CH9 are servo targets.
 * CH10..CH15 are reserved for the future ESC service.
 * CH16..CH19 are application-reserved.
 *
 * During this phase all non-servo channels have an explicit disabled state.
 * This is their current software state, not an "ignore" placeholder and not
 * the future ESC neutral definition.
 */
static uint8_t channel_angle[LU9685_CHANNEL_COUNT];

void LU9685_ChannelState_Init(void)
{
    uint8_t channel;

    for (channel = 0U; channel < LU9685_CHANNEL_COUNT; ++channel)
    {
        channel_angle[channel] = LU9685_OUTPUT_DISABLED;
    }

    for (channel = LU9685_SERVO_CHANNEL_FIRST;
         channel <= LU9685_SERVO_CHANNEL_LAST;
         ++channel)
    {
        channel_angle[channel] = LU9685_SERVO_INITIAL_ANGLE;
    }
}

void LU9685_ChannelState_Copy(uint8_t output[LU9685_CHANNEL_COUNT])
{
    if (output != NULL)
    {
        memcpy(output, channel_angle, LU9685_CHANNEL_COUNT);
    }
}

uint8_t LU9685_ChannelState_Get(uint8_t channel)
{
    return (channel < LU9685_CHANNEL_COUNT)
               ? channel_angle[channel]
               : LU9685_OUTPUT_DISABLED;
}

void LU9685_ChannelState_CommitServo(uint8_t channel, uint8_t angle)
{
    if ((channel >= LU9685_SERVO_CHANNEL_FIRST) &&
        (channel <= LU9685_SERVO_CHANNEL_LAST) &&
        (angle <= LU9685_MAX_ANGLE))
    {
        channel_angle[channel] = angle;
    }
}

void LU9685_ChannelState_CommitAllServos(uint8_t angle)
{
    uint8_t channel;

    if (angle > LU9685_MAX_ANGLE)
    {
        return;
    }

    for (channel = LU9685_SERVO_CHANNEL_FIRST;
         channel <= LU9685_SERVO_CHANNEL_LAST;
         ++channel)
    {
        channel_angle[channel] = angle;
    }
}

void LU9685_ChannelState_CommitRange(uint8_t first_channel,
                                    const uint8_t *angles,
                                    uint8_t count)
{
    uint8_t index;

    if ((angles == NULL) || (count == 0U) ||
        (first_channel >= LU9685_CHANNEL_COUNT) ||
        (count > (LU9685_CHANNEL_COUNT - first_channel)))
    {
        return;
    }

    for (index = 0U; index < count; ++index)
    {
        if (angles[index] > LU9685_MAX_ANGLE)
        {
            return;
        }
    }

    memcpy(&channel_angle[first_channel], angles, count);
}
