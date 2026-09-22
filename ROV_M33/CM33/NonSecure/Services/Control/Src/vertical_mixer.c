/**
  ******************************************************************************
  * @file    vertical_mixer.c
  * @brief   Roll/pitch mixer for vertical thruster commands CH10..CH13.
  ******************************************************************************
  */

#include "vertical_mixer.h"

#include <math.h>
#include <stddef.h>

static float VerticalMixer_Clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

VerticalMixer_StatusTypeDef VerticalMixer_Mix(
    int16_t vertical_base,
    float roll_output,
    float pitch_output,
    int16_t commands[VERTICAL_MIXER_CHANNEL_COUNT])
{
    float correction[VERTICAL_MIXER_CHANNEL_COUNT];
    float final_command;
    uint8_t index;

    if ((commands == NULL) ||
        (vertical_base < VERTICAL_MIXER_COMMAND_MIN) ||
        (vertical_base > VERTICAL_MIXER_COMMAND_MAX))
    {
        return VERTICAL_MIXER_STATUS_BAD_ARG;
    }

    correction[0] = roll_output - pitch_output;
    correction[1] = roll_output + pitch_output;
    correction[2] = -roll_output - pitch_output;
    correction[3] = -roll_output + pitch_output;

    for (index = 0U; index < VERTICAL_MIXER_CHANNEL_COUNT; ++index)
    {
        correction[index] = VerticalMixer_Clamp(
            correction[index],
            -VERTICAL_MIXER_CORRECTION_LIMIT,
            VERTICAL_MIXER_CORRECTION_LIMIT);
        final_command = VerticalMixer_Clamp(
            (float)vertical_base + correction[index],
            (float)VERTICAL_MIXER_COMMAND_MIN,
            (float)VERTICAL_MIXER_COMMAND_MAX);
        commands[index] = (int16_t)lroundf(final_command);
    }

    return VERTICAL_MIXER_STATUS_OK;
}
