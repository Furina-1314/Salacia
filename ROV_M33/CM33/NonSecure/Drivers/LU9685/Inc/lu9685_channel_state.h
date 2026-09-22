/**
  ******************************************************************************
  * @file    lu9685_channel_state.h
  * @brief   Ownership-aware software state for all 20 LU9685 channels.
  ******************************************************************************
  */

#ifndef LU9685_CHANNEL_STATE_H
#define LU9685_CHANNEL_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lu9685.h"

#define LU9685_SERVO_CHANNEL_FIRST       0U
#define LU9685_SERVO_CHANNEL_LAST        9U

/* Formal hardware mapping only; no propeller control is implemented here. */
typedef enum
{
    PROP_LEFT_FRONT_VERTICAL = 10,
    PROP_LEFT_BACK_VERTICAL = 11,
    PROP_RIGHT_FRONT_VERTICAL = 12,
    PROP_RIGHT_BACK_VERTICAL = 13,
    PROP_LEFT_HORIZONTAL = 14,
    PROP_RIGHT_HORIZONTAL = 15
} PropellerChannel;

#define LU9685_ESC_CHANNEL_FIRST         ((uint8_t)PROP_LEFT_FRONT_VERTICAL)
#define LU9685_ESC_CHANNEL_LAST          ((uint8_t)PROP_RIGHT_HORIZONTAL)
#define LU9685_RESERVED_CHANNEL_FIRST    16U
#define LU9685_RESERVED_CHANNEL_LAST     19U

void LU9685_ChannelState_Init(void);
void LU9685_ChannelState_Copy(uint8_t output[LU9685_CHANNEL_COUNT]);
uint8_t LU9685_ChannelState_Get(uint8_t channel);
void LU9685_ChannelState_CommitServo(uint8_t channel, uint8_t angle);
void LU9685_ChannelState_CommitAllServos(uint8_t angle);
void LU9685_ChannelState_CommitRange(uint8_t first_channel,
                                    const uint8_t *angles,
                                    uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* LU9685_CHANNEL_STATE_H */
