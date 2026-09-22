/**
  ******************************************************************************
  * @file    propeller_service.h
  * @brief   Basic six-thruster service for actuator channels CH10..CH15.
  ******************************************************************************
  */

#ifndef PROPELLER_SERVICE_H
#define PROPELLER_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PROPELLER_SERVICE_VERTICAL_FIRST       10U
#define PROPELLER_SERVICE_VERTICAL_LAST        13U
#define PROPELLER_SERVICE_HORIZONTAL_FIRST     14U
#define PROPELLER_SERVICE_HORIZONTAL_LAST      15U
#define PROPELLER_SERVICE_CHANNEL_FIRST         10U
#define PROPELLER_SERVICE_CHANNEL_LAST          15U
#define PROPELLER_SERVICE_CHANNEL_COUNT          6U
#define PROPELLER_SERVICE_COMMAND_MIN          (-100)
#define PROPELLER_SERVICE_COMMAND_MAX           100
#define PROPELLER_SERVICE_PWM_MIN_US            1000U
#define PROPELLER_SERVICE_PWM_NEUTRAL_US        1500U
#define PROPELLER_SERVICE_PWM_MAX_US            2000U

typedef enum
{
    PROPELLER_SERVICE_STATUS_OK = 0,
    PROPELLER_SERVICE_STATUS_BAD_ARG,
    PROPELLER_SERVICE_STATUS_SAFETY,
    PROPELLER_SERVICE_STATUS_NOT_READY,
    PROPELLER_SERVICE_STATUS_IO_ERROR
} PropellerService_StatusTypeDef;

typedef struct
{
    int16_t vertical_base;
    int16_t last_vertical_base;
    int16_t vertical[4];
    int16_t horizontal_base;
    int16_t horizontal[2];
    int16_t real[PROPELLER_SERVICE_CHANNEL_COUNT];
    uint8_t horizontal_enabled;
    uint8_t synchronization_enabled;
    uint8_t global_stopped;
    uint8_t vertical_stopped;
    uint8_t horizontal_stopped;
} PropellerService_StateTypeDef;

PropellerService_StatusTypeDef PropellerService_Init(void);
PropellerService_StatusTypeDef PropellerService_SetHorizontalEnabled(
    uint8_t enabled);
PropellerService_StatusTypeDef PropellerService_SetSynchronizationEnabled(
    uint8_t enabled);
PropellerService_StatusTypeDef PropellerService_SetVerticalBase(int16_t command);
PropellerService_StatusTypeDef PropellerService_SetVertical(uint8_t channel,
                                                            int16_t command);
PropellerService_StatusTypeDef PropellerService_SetHorizontalBase(
    int16_t command);
PropellerService_StatusTypeDef PropellerService_SetHorizontal(uint8_t channel,
                                                              int16_t command);
PropellerService_StatusTypeDef PropellerService_StopAll(void);
PropellerService_StatusTypeDef PropellerService_MoveAll(void);
PropellerService_StatusTypeDef PropellerService_StopVertical(void);
PropellerService_StatusTypeDef PropellerService_MoveVertical(void);
PropellerService_StatusTypeDef PropellerService_StopHorizontal(void);
PropellerService_StatusTypeDef PropellerService_MoveHorizontal(void);
PropellerService_StatusTypeDef PropellerService_ApplyVerticalControl(
    const int16_t commands[4]);
PropellerService_StatusTypeDef PropellerService_GetBase(uint8_t channel,
                                                        int16_t *command);
PropellerService_StatusTypeDef PropellerService_GetAllBase(
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT]);
PropellerService_StatusTypeDef PropellerService_GetReal(uint8_t channel,
                                                        int16_t *command);
PropellerService_StatusTypeDef PropellerService_GetAllReal(
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT]);
PropellerService_StatusTypeDef PropellerService_GetState(
    PropellerService_StateTypeDef *state);
PropellerService_StatusTypeDef PropellerService_CommandToPwmUs(
    int16_t command,
    uint16_t *pwm_us);
#ifdef __cplusplus
}
#endif

#endif /* PROPELLER_SERVICE_H */
