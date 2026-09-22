/**
  ******************************************************************************
  * @file    vertical_mixer.h
  * @brief   Roll/pitch mixer for vertical thruster commands CH10..CH13.
  ******************************************************************************
  */

#ifndef VERTICAL_MIXER_H
#define VERTICAL_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define VERTICAL_MIXER_CHANNEL_COUNT          4U
#define VERTICAL_MIXER_CORRECTION_LIMIT       10.0f
#define VERTICAL_MIXER_COMMAND_MIN           (-100)
#define VERTICAL_MIXER_COMMAND_MAX            100

typedef enum
{
    VERTICAL_MIXER_STATUS_OK = 0,
    VERTICAL_MIXER_STATUS_BAD_ARG
} VerticalMixer_StatusTypeDef;

VerticalMixer_StatusTypeDef VerticalMixer_Mix(
    int16_t vertical_base,
    float roll_output,
    float pitch_output,
    int16_t commands[VERTICAL_MIXER_CHANNEL_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* VERTICAL_MIXER_H */
