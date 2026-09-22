#include "propeller_service.h"
#include "actuator_service.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t submitted_first;
static uint8_t submitted_count;
static uint16_t submitted_pulse_us[PROPELLER_SERVICE_CHANNEL_COUNT];
static ActuatorService_StatusTypeDef next_status;

ActuatorService_StatusTypeDef ActuatorService_SetChannelPulseUs(
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count)
{
    ActuatorService_StatusTypeDef result = next_status;

    submitted_first = first_channel;
    submitted_count = count;
    memset(submitted_pulse_us, 0, sizeof(submitted_pulse_us));
    memcpy(submitted_pulse_us, pulse_us,
           (size_t)count * sizeof(pulse_us[0]));
    next_status = ACTUATOR_SERVICE_STATUS_OK;
    return result;
}

static void expect_submission(uint8_t first, uint8_t count,
                              const uint16_t *expected)
{
    assert(submitted_first == first);
    assert(submitted_count == count);
    assert(memcmp(submitted_pulse_us, expected,
                  (size_t)count * sizeof(expected[0])) == 0);
}

static void test_mapping(void)
{
    static const int16_t commands[] = {-100, -50, 0, 50, 100};
    static const uint16_t pwms[] = {1000, 1250, 1500, 1750, 2000};
    uint16_t pwm;
    size_t index;

    for (index = 0U; index < (sizeof(commands) / sizeof(commands[0])); ++index)
    {
        assert(PropellerService_CommandToPwmUs(commands[index], &pwm) ==
               PROPELLER_SERVICE_STATUS_OK);
        assert(pwm == pwms[index]);
    }
    assert(PropellerService_CommandToPwmUs(-101, &pwm) ==
           PROPELLER_SERVICE_STATUS_BAD_ARG);
    assert(PropellerService_CommandToPwmUs(101, &pwm) ==
           PROPELLER_SERVICE_STATUS_BAD_ARG);
}

static void test_vertical_modes_and_write_width(void)
{
    static const uint16_t neutral6[] = {1500, 1500, 1500, 1500, 1500, 1500};
    static const uint16_t base40_4[] = {1700, 1700, 1700, 1700};
    static const uint16_t single20[] = {1600};
    PropellerService_StateTypeDef state;
    uint8_t index;

    next_status = ACTUATOR_SERVICE_STATUS_OK;
    assert(PropellerService_Init() == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 6U, neutral6);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.horizontal_enabled == 1U);

    /* Horizontal defaults ON, so base updates all four vertical outputs. */
    assert(PropellerService_SetVerticalBase(40) == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 4U, base40_4);
    assert(PropellerService_SetVertical(10U, 20) ==
           PROPELLER_SERVICE_STATUS_SAFETY);

    /* ON -> OFF performs no write and seeds individual state from real output. */
    submitted_count = 0U;
    assert(PropellerService_SetHorizontalEnabled(0U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(submitted_count == 0U);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.horizontal_enabled == 0U);
    assert(state.last_vertical_base == 40);
    for (index = 0U; index < 4U; ++index)
    {
        assert(state.vertical[index] == 40);
        assert(state.real[index] == 40);
    }

    /* Individual control continues from the held output without a jump. */
    assert(PropellerService_SetVertical(10U, 20) == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 1U, single20);

    /* OFF -> ON restores last_vertical_base on all four channels. */
    assert(PropellerService_SetHorizontalEnabled(1U) ==
           PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 4U, base40_4);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.horizontal_enabled == 1U);
    assert(state.vertical_base == 40);
    assert(state.last_vertical_base == 40);
    for (index = 0U; index < 4U; ++index)
    {
        assert(state.real[index] == 40);
    }
}

static void test_horizontal_modes_queries_and_stop(void)
{
    static const uint16_t base40_2[] = {1700, 1700};
    static const uint16_t base_minus20_2[] = {1400, 1400};
    static const uint16_t single20[] = {1600};
    static const uint16_t neutral6[] = {1500, 1500, 1500, 1500, 1500, 1500};
    PropellerService_StateTypeDef before;
    PropellerService_StateTypeDef after;
    int16_t commands[PROPELLER_SERVICE_CHANNEL_COUNT];
    int16_t command;
    uint8_t index;

    assert(PropellerService_Init() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetHorizontalBase(40) == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(14U, 2U, base40_2);
    assert(PropellerService_SetHorizontal(14U, 20) ==
           PROPELLER_SERVICE_STATUS_SAFETY);
    assert(PropellerService_GetBase(14U, &command) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(command == 40);

    assert(PropellerService_SetSynchronizationEnabled(0U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetHorizontalBase(-20) ==
           PROPELLER_SERVICE_STATUS_OK);
    expect_submission(14U, 2U, base_minus20_2);
    assert(PropellerService_SetHorizontal(14U, 20) ==
           PROPELLER_SERVICE_STATUS_OK);
    expect_submission(14U, 1U, single20);
    assert(PropellerService_GetBase(14U, &command) ==
           PROPELLER_SERVICE_STATUS_SAFETY);
    assert(PropellerService_GetReal(14U, &command) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(command == 20);

    assert(PropellerService_GetState(&before) == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_StopAll() == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 6U, neutral6);
    assert(PropellerService_GetState(&after) == PROPELLER_SERVICE_STATUS_OK);
    assert(before.vertical_base == after.vertical_base);
    assert(before.last_vertical_base == after.last_vertical_base);
    assert(memcmp(before.vertical, after.vertical, sizeof(before.vertical)) == 0);
    assert(before.horizontal_base == after.horizontal_base);
    assert(memcmp(before.horizontal, after.horizontal,
                  sizeof(before.horizontal)) == 0);
    assert(before.horizontal_enabled == after.horizontal_enabled);
    assert(before.synchronization_enabled == after.synchronization_enabled);
    assert(PropellerService_GetAllReal(commands) == PROPELLER_SERVICE_STATUS_OK);
    for (index = 0U; index < PROPELLER_SERVICE_CHANNEL_COUNT; ++index)
    {
        assert(commands[index] == 0);
    }
}

static void test_failed_output_rolls_back_state(void)
{
    PropellerService_StateTypeDef before;
    PropellerService_StateTypeDef after;
    int16_t control[4] = {1, 2, 3, 4};

    assert(PropellerService_Init() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetHorizontalEnabled(0U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_GetState(&before) == PROPELLER_SERVICE_STATUS_OK);
    next_status = ACTUATOR_SERVICE_STATUS_IO_ERROR;
    assert(PropellerService_SetVertical(10U, 70) ==
           PROPELLER_SERVICE_STATUS_IO_ERROR);
    assert(PropellerService_GetState(&after) == PROPELLER_SERVICE_STATUS_OK);
    assert(memcmp(&before, &after, sizeof(before)) == 0);

    assert(PropellerService_Init() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_GetState(&before) == PROPELLER_SERVICE_STATUS_OK);
    next_status = ACTUATOR_SERVICE_STATUS_IO_ERROR;
    assert(PropellerService_ApplyVerticalControl(control) ==
           PROPELLER_SERVICE_STATUS_IO_ERROR);
    assert(PropellerService_GetState(&after) == PROPELLER_SERVICE_STATUS_OK);
    assert(memcmp(&before, &after, sizeof(before)) == 0);

    next_status = ACTUATOR_SERVICE_STATUS_IO_ERROR;
    assert(PropellerService_StopAll() == PROPELLER_SERVICE_STATUS_IO_ERROR);
    assert(PropellerService_GetState(&after) == PROPELLER_SERVICE_STATUS_OK);
    assert(memcmp(&before, &after, sizeof(before)) == 0);

    next_status = ACTUATOR_SERVICE_STATUS_IO_ERROR;
    assert(PropellerService_SetVerticalBase(50) ==
           PROPELLER_SERVICE_STATUS_IO_ERROR);
    assert(PropellerService_GetState(&after) == PROPELLER_SERVICE_STATUS_OK);
    assert(memcmp(&before, &after, sizeof(before)) == 0);
}

static void test_stop_move_and_control_api(void)
{
    static const uint16_t vertical30[] = {1650, 1650, 1650, 1650};
    static const uint16_t control[] = {1550, 1600, 1450, 1400};
    static const uint16_t neutral4[] = {1500, 1500, 1500, 1500};
    static const uint16_t neutral2[] = {1500, 1500};
    int16_t control_commands[4] = {10, 20, -10, -20};
    PropellerService_StateTypeDef state;

    assert(PropellerService_Init() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetVerticalBase(30) == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 4U, vertical30);
    assert(PropellerService_ApplyVerticalControl(control_commands) ==
           PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 4U, control);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.vertical_base == 30);
    assert(state.last_vertical_base == 0);

    assert(PropellerService_SetHorizontalEnabled(0U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetVertical(10U, 55) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetHorizontalEnabled(1U) ==
           PROPELLER_SERVICE_STATUS_OK);

    assert(PropellerService_StopVertical() == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(10U, 4U, neutral4);
    assert(PropellerService_SetVerticalBase(40) ==
           PROPELLER_SERVICE_STATUS_SAFETY);
    assert(PropellerService_SetHorizontalEnabled(0U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.vertical[0] == 55); /* Stopped real=0 cannot erase targets. */
    assert(PropellerService_SetHorizontalEnabled(1U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_ApplyVerticalControl(control_commands) ==
           PROPELLER_SERVICE_STATUS_SAFETY);
    assert(PropellerService_MoveVertical() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.real[0] == 0 && state.vertical_base == 30);

    assert(PropellerService_StopHorizontal() == PROPELLER_SERVICE_STATUS_OK);
    expect_submission(14U, 2U, neutral2);
    assert(PropellerService_SetHorizontalBase(10) ==
           PROPELLER_SERVICE_STATUS_SAFETY);
    assert(PropellerService_MoveHorizontal() == PROPELLER_SERVICE_STATUS_OK);

    assert(PropellerService_StopVertical() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_StopAll() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_MoveAll() == PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_SetVerticalBase(20) ==
           PROPELLER_SERVICE_STATUS_SAFETY);
    assert(PropellerService_SetHorizontalBase(20) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.global_stopped == 0U);
    assert(state.vertical_stopped == 1U);
    assert(state.horizontal_stopped == 0U);

    /* Mode changes do not clear a stop and do not write a non-zero output. */
    assert(PropellerService_SetHorizontalEnabled(0U) ==
           PROPELLER_SERVICE_STATUS_OK);
    submitted_count = 0U;
    assert(PropellerService_SetHorizontalEnabled(1U) ==
           PROPELLER_SERVICE_STATUS_OK);
    assert(submitted_count == 0U);
    assert(PropellerService_GetState(&state) == PROPELLER_SERVICE_STATUS_OK);
    assert(state.vertical_stopped == 1U);
}

int main(void)
{
    test_mapping();
    test_vertical_modes_and_write_width();
    test_horizontal_modes_queries_and_stop();
    test_failed_output_rolls_back_state();
    test_stop_move_and_control_api();
    puts("propeller_service_test: PASS");
    return 0;
}
