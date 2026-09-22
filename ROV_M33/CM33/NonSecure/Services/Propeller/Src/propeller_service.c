/**
  ******************************************************************************
  * @file    propeller_service.c
  * @brief   Six-thruster state and output service for PCA9685 CH10..CH15.
  ******************************************************************************
  */

#include "propeller_service.h"

#include "actuator_service.h"

#include <string.h>

static PropellerService_StateTypeDef propeller_state;
static uint8_t propeller_initialized;

static uint8_t PropellerService_IsValidCommand(int16_t command)
{
    return (command >= PROPELLER_SERVICE_COMMAND_MIN) &&
           (command <= PROPELLER_SERVICE_COMMAND_MAX);
}

static uint8_t PropellerService_VerticalBlocked(void)
{
    return (propeller_state.global_stopped != 0U) ||
           (propeller_state.vertical_stopped != 0U);
}

static uint8_t PropellerService_HorizontalBlocked(void)
{
    return (propeller_state.global_stopped != 0U) ||
           (propeller_state.horizontal_stopped != 0U);
}

static PropellerService_StatusTypeDef PropellerService_MapActuatorStatus(
    ActuatorService_StatusTypeDef status)
{
    switch (status)
    {
        case ACTUATOR_SERVICE_STATUS_OK:
            return PROPELLER_SERVICE_STATUS_OK;
        case ACTUATOR_SERVICE_STATUS_BAD_ARG:
            return PROPELLER_SERVICE_STATUS_BAD_ARG;
        case ACTUATOR_SERVICE_STATUS_NOT_READY:
            return PROPELLER_SERVICE_STATUS_NOT_READY;
        default:
            return PROPELLER_SERVICE_STATUS_IO_ERROR;
    }
}

PropellerService_StatusTypeDef PropellerService_CommandToPwmUs(
    int16_t command,
    uint16_t *pwm_us)
{
    if ((pwm_us == NULL) || (PropellerService_IsValidCommand(command) == 0U))
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }

    *pwm_us = (uint16_t)((int32_t)PROPELLER_SERVICE_PWM_NEUTRAL_US +
                         (5 * (int32_t)command));
    return PROPELLER_SERVICE_STATUS_OK;
}

static PropellerService_StatusTypeDef PropellerService_SubmitRange(
    uint8_t first_channel,
    const int16_t *commands,
    uint8_t count)
{
    uint16_t pulse_us[PROPELLER_SERVICE_CHANNEL_COUNT];
    uint8_t index;

    if ((commands == NULL) || (count == 0U) ||
        (count > PROPELLER_SERVICE_CHANNEL_COUNT))
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }

    for (index = 0U; index < count; ++index)
    {
        if (PropellerService_CommandToPwmUs(commands[index], &pulse_us[index]) !=
            PROPELLER_SERVICE_STATUS_OK)
        {
            return PROPELLER_SERVICE_STATUS_BAD_ARG;
        }
    }

    return PropellerService_MapActuatorStatus(
        ActuatorService_SetChannelPulseUs(first_channel, pulse_us, count));
}

static PropellerService_StatusTypeDef PropellerService_CommitRange(
    PropellerService_StateTypeDef *candidate,
    uint8_t first_channel,
    const int16_t *commands,
    uint8_t count)
{
    PropellerService_StatusTypeDef status;
    uint8_t index;
    uint8_t real_index;

    status = PropellerService_SubmitRange(first_channel, commands, count);
    if (status != PROPELLER_SERVICE_STATUS_OK)
    {
        return status;
    }

    real_index = first_channel - PROPELLER_SERVICE_CHANNEL_FIRST;
    for (index = 0U; index < count; ++index)
    {
        candidate->real[real_index + index] = commands[index];
    }
    propeller_state = *candidate;
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_Init(void)
{
    PropellerService_StateTypeDef initial_state;
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT] = {0};
    PropellerService_StatusTypeDef status;

    memset(&initial_state, 0, sizeof(initial_state));
    initial_state.horizontal_enabled = 1U;
    initial_state.synchronization_enabled = 1U;

    status = PropellerService_SubmitRange(PROPELLER_SERVICE_CHANNEL_FIRST,
                                          commands,
                                          PROPELLER_SERVICE_CHANNEL_COUNT);
    if (status == PROPELLER_SERVICE_STATUS_OK)
    {
        propeller_state = initial_state;
        propeller_initialized = 1U;
    }
    else
    {
        propeller_initialized = 0U;
    }
    return status;
}

PropellerService_StatusTypeDef PropellerService_SetHorizontalEnabled(
    uint8_t enabled)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[4];
    uint8_t index;

    if (enabled > 1U)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (enabled == propeller_state.horizontal_enabled)
    {
        return PROPELLER_SERVICE_STATUS_OK;
    }

    candidate = propeller_state;
    if (enabled != 0U)
    {
        candidate.vertical_base = candidate.last_vertical_base;
        for (index = 0U; index < 4U; ++index)
        {
            commands[index] = candidate.vertical_base;
        }
    }
    else
    {
        candidate.last_vertical_base = candidate.vertical_base;
        if (PropellerService_VerticalBlocked() == 0U)
        {
            memcpy(candidate.vertical, candidate.real, sizeof(commands));
        }
        candidate.horizontal_enabled = enabled;
        propeller_state = candidate;
        return PROPELLER_SERVICE_STATUS_OK;
    }
    candidate.horizontal_enabled = enabled;
    if (PropellerService_VerticalBlocked() != 0U)
    {
        propeller_state = candidate;
        return PROPELLER_SERVICE_STATUS_OK;
    }
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_VERTICAL_FIRST,
                                        commands,
                                        4U);
}

PropellerService_StatusTypeDef PropellerService_SetSynchronizationEnabled(
    uint8_t enabled)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[2];

    if (enabled > 1U)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (enabled == propeller_state.synchronization_enabled)
    {
        return PROPELLER_SERVICE_STATUS_OK;
    }

    candidate = propeller_state;
    candidate.synchronization_enabled = enabled;
    if (enabled != 0U)
    {
        commands[0] = candidate.horizontal_base;
        commands[1] = candidate.horizontal_base;
    }
    else
    {
        memcpy(commands, candidate.horizontal, sizeof(commands));
    }
    if (PropellerService_HorizontalBlocked() != 0U)
    {
        propeller_state = candidate;
        return PROPELLER_SERVICE_STATUS_OK;
    }
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_HORIZONTAL_FIRST,
                                        commands,
                                        2U);
}

PropellerService_StatusTypeDef PropellerService_SetVerticalBase(int16_t command)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[4];
    uint8_t index;

    if (PropellerService_IsValidCommand(command) == 0U)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (PropellerService_VerticalBlocked() != 0U)
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }

    candidate = propeller_state;
    for (index = 0U; index < 4U; ++index)
    {
        commands[index] = command;
    }
    if (candidate.horizontal_enabled != 0U)
    {
        candidate.vertical_base = command;
    }
    else
    {
        for (index = 0U; index < 4U; ++index)
        {
            candidate.vertical[index] = command;
        }
    }
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_VERTICAL_FIRST,
                                        commands,
                                        4U);
}

PropellerService_StatusTypeDef PropellerService_SetVertical(uint8_t channel,
                                                            int16_t command)
{
    PropellerService_StateTypeDef candidate;

    if ((channel < PROPELLER_SERVICE_VERTICAL_FIRST) ||
        (channel > PROPELLER_SERVICE_VERTICAL_LAST) ||
        (PropellerService_IsValidCommand(command) == 0U))
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (PropellerService_VerticalBlocked() != 0U)
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }
    if (propeller_state.horizontal_enabled != 0U)
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }

    candidate = propeller_state;
    candidate.vertical[channel - PROPELLER_SERVICE_VERTICAL_FIRST] = command;
    return PropellerService_CommitRange(&candidate, channel, &command, 1U);
}

PropellerService_StatusTypeDef PropellerService_SetHorizontalBase(
    int16_t command)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[2] = {command, command};

    if (PropellerService_IsValidCommand(command) == 0U)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (PropellerService_HorizontalBlocked() != 0U)
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }

    candidate = propeller_state;
    if (candidate.synchronization_enabled != 0U)
    {
        candidate.horizontal_base = command;
    }
    else
    {
        candidate.horizontal[0] = command;
        candidate.horizontal[1] = command;
    }
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_HORIZONTAL_FIRST,
                                        commands,
                                        2U);
}

PropellerService_StatusTypeDef PropellerService_SetHorizontal(uint8_t channel,
                                                              int16_t command)
{
    PropellerService_StateTypeDef candidate;

    if ((channel < PROPELLER_SERVICE_HORIZONTAL_FIRST) ||
        (channel > PROPELLER_SERVICE_HORIZONTAL_LAST) ||
        (PropellerService_IsValidCommand(command) == 0U))
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (PropellerService_HorizontalBlocked() != 0U)
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }
    if (propeller_state.synchronization_enabled != 0U)
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }

    candidate = propeller_state;
    candidate.horizontal[channel - PROPELLER_SERVICE_HORIZONTAL_FIRST] = command;
    return PropellerService_CommitRange(&candidate, channel, &command, 1U);
}

PropellerService_StatusTypeDef PropellerService_StopAll(void)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT] = {0};

    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }

    candidate = propeller_state;
    candidate.global_stopped = 1U;
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_CHANNEL_FIRST,
                                        commands,
                                        PROPELLER_SERVICE_CHANNEL_COUNT);
}

PropellerService_StatusTypeDef PropellerService_MoveAll(void)
{
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    propeller_state.global_stopped = 0U;
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_StopVertical(void)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[4] = {0};

    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    candidate = propeller_state;
    candidate.vertical_stopped = 1U;
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_VERTICAL_FIRST,
                                        commands, 4U);
}

PropellerService_StatusTypeDef PropellerService_MoveVertical(void)
{
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    propeller_state.vertical_stopped = 0U;
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_StopHorizontal(void)
{
    PropellerService_StateTypeDef candidate;
    int16_t commands[2] = {0};

    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    candidate = propeller_state;
    candidate.horizontal_stopped = 1U;
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_HORIZONTAL_FIRST,
                                        commands, 2U);
}

PropellerService_StatusTypeDef PropellerService_MoveHorizontal(void)
{
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    propeller_state.horizontal_stopped = 0U;
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_ApplyVerticalControl(
    const int16_t commands[4])
{
    PropellerService_StateTypeDef candidate;
    uint8_t index;

    if (commands == NULL)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if ((propeller_state.horizontal_enabled == 0U) ||
        (PropellerService_VerticalBlocked() != 0U))
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }
    for (index = 0U; index < 4U; ++index)
    {
        if (PropellerService_IsValidCommand(commands[index]) == 0U)
        {
            return PROPELLER_SERVICE_STATUS_BAD_ARG;
        }
    }

    candidate = propeller_state;
    return PropellerService_CommitRange(&candidate,
                                        PROPELLER_SERVICE_VERTICAL_FIRST,
                                        commands, 4U);
}

PropellerService_StatusTypeDef PropellerService_GetBase(uint8_t channel,
                                                        int16_t *command)
{
    if ((command == NULL) ||
        (channel < PROPELLER_SERVICE_CHANNEL_FIRST) ||
        (channel > PROPELLER_SERVICE_CHANNEL_LAST))
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if (channel <= PROPELLER_SERVICE_VERTICAL_LAST)
    {
        if (propeller_state.horizontal_enabled == 0U)
        {
            return PROPELLER_SERVICE_STATUS_SAFETY;
        }
        *command = propeller_state.vertical_base;
    }
    else
    {
        if (propeller_state.synchronization_enabled == 0U)
        {
            return PROPELLER_SERVICE_STATUS_SAFETY;
        }
        *command = propeller_state.horizontal_base;
    }
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_GetAllBase(
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT])
{
    uint8_t index;

    if (commands == NULL)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }
    if ((propeller_state.horizontal_enabled == 0U) ||
        (propeller_state.synchronization_enabled == 0U))
    {
        return PROPELLER_SERVICE_STATUS_SAFETY;
    }

    for (index = 0U; index < 4U; ++index)
    {
        commands[index] = propeller_state.vertical_base;
    }
    commands[4] = propeller_state.horizontal_base;
    commands[5] = propeller_state.horizontal_base;
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_GetReal(uint8_t channel,
                                                        int16_t *command)
{
    if ((command == NULL) ||
        (channel < PROPELLER_SERVICE_CHANNEL_FIRST) ||
        (channel > PROPELLER_SERVICE_CHANNEL_LAST))
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }

    *command = propeller_state.real[channel - PROPELLER_SERVICE_CHANNEL_FIRST];
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_GetAllReal(
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT])
{
    if (commands == NULL)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }

    memcpy(commands, propeller_state.real, sizeof(propeller_state.real));
    return PROPELLER_SERVICE_STATUS_OK;
}

PropellerService_StatusTypeDef PropellerService_GetState(
    PropellerService_StateTypeDef *state)
{
    if (state == NULL)
    {
        return PROPELLER_SERVICE_STATUS_BAD_ARG;
    }
    if (propeller_initialized == 0U)
    {
        return PROPELLER_SERVICE_STATUS_NOT_READY;
    }

    *state = propeller_state;
    return PROPELLER_SERVICE_STATUS_OK;
}
