#include "stability_control.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned int apply_count;
static int16_t applied[4];
static PropellerService_StatusTypeDef next_status;

PropellerService_StatusTypeDef PropellerService_ApplyVerticalControl(
    const int16_t commands[4])
{
    PropellerService_StatusTypeDef status = next_status;
    ++apply_count;
    memcpy(applied, commands, sizeof(applied));
    next_status = PROPELLER_SERVICE_STATUS_OK;
    return status;
}

int main(void)
{
    PropellerService_StateTypeDef state;
    StabilityControl_TelemetryTypeDef telemetry;

    memset(&state, 0, sizeof(state));
    state.horizontal_enabled = 1U;
    state.vertical_base = 25;
    assert(StabilityControl_Init() == STABILITY_CONTROL_STATUS_OK);
    assert(StabilityControl_SyncState(&state) == STABILITY_CONTROL_STATUS_OK);

    assert(StabilityControl_Execute(20.0f, 0.0f, 0U, 0U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_OK);
    assert(apply_count == 0U);

    assert(StabilityControl_Execute(20.0f, 0.0f, 1U, 1U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_OK);
    assert(apply_count == 1U);
    assert(applied[0] < 25 && applied[1] < 25);
    assert(applied[2] > 25 && applied[3] > 25);

    assert(StabilityControl_Execute(20.0f, 0.0f, 1U, 0U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_OK);
    assert(apply_count == 2U);
    assert(applied[0] == 25 && applied[1] == 25 &&
           applied[2] == 25 && applied[3] == 25);

    state.vertical_stopped = 1U;
    assert(StabilityControl_Execute(20.0f, 0.0f, 1U, 1U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_SAFETY);
    assert(apply_count == 2U);
    state.vertical_stopped = 0U;
    state.global_stopped = 1U;
    assert(StabilityControl_Execute(20.0f, 0.0f, 1U, 0U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_SAFETY);
    assert(apply_count == 2U);

    state.global_stopped = 0U;
    state.horizontal_enabled = 0U;
    assert(StabilityControl_SyncState(&state) == STABILITY_CONTROL_STATUS_OK);
    state.horizontal_enabled = 1U;
    assert(StabilityControl_SyncState(&state) == STABILITY_CONTROL_STATUS_OK);
    assert(StabilityControl_Execute(5.0f, 0.0f, 1U, 1U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_OK);
    assert(applied[0] == 25 && applied[1] == 25 &&
           applied[2] == 25 && applied[3] == 25);

    next_status = PROPELLER_SERVICE_STATUS_IO_ERROR;
    assert(StabilityControl_Execute(20.0f, 0.0f, 1U, 1U, &state, 0.02f) ==
           STABILITY_CONTROL_STATUS_IO_ERROR);
    assert(StabilityControl_GetTelemetry(&telemetry) ==
           STABILITY_CONTROL_STATUS_OK);
    puts("stability_control_test: PASS");
    return 0;
}
