/**
  ******************************************************************************
  * @file    dyp.h
  * @brief   Non-blocking DYP-L08 UART controlled-output driver.
  ******************************************************************************
  */

#ifndef DYP_H
#define DYP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32mp2xx_hal.h"

#define DYP_FRAME_SIZE                 4U
/* Target at least about 5 ms LOW, matching the verified ESP32 setup. The
 * extra tick covers up to one 1 ms HAL tick of quantization; main-loop
 * scheduling can only extend the pulse. */
#define DYP_TRIGGER_LOW_TARGET_MS      5U
#define DYP_TRIGGER_PULSE_MS          (DYP_TRIGGER_LOW_TARGET_MS + 1U)
#define DYP_TRIGGER_MIN_INTERVAL_MS   34U
/* The module answers after the rising trigger edge, and the main loop
 * releases the trigger with jitter under load (e.g. 100 Hz MPU polling via
 * RPMsg). 60 ms only passed on an idle core; 200 ms keeps the gateway's
 * 5 s polling reliable while staying far below the poll period. */
#define DYP_RESPONSE_TIMEOUT_MS       200U

typedef enum
{
    DYP_STATUS_OK = 0,
    DYP_STATUS_BAD_ARG,
    DYP_STATUS_NOT_READY,
    DYP_STATUS_BUSY,
    DYP_STATUS_TIMEOUT,
    DYP_STATUS_IO_ERROR
} DYP_StatusTypeDef;

typedef enum
{
    DYP_STATE_UNINITIALIZED = 0,
    DYP_STATE_IDLE,
    DYP_STATE_WAITING,
    DYP_STATE_COMPLETE,
    DYP_STATE_TIMEOUT,
    DYP_STATE_IO_ERROR
} DYP_StateTypeDef;

typedef struct
{
    DYP_StateTypeDef state;
    uint16_t distance_mm;
    uint32_t age_ms;
    uint32_t last_hal_error;
    uint8_t ready;
    uint8_t busy;
    uint8_t valid;
} DYP_SnapshotTypeDef;

DYP_StatusTypeDef DYP_Init(void);
void DYP_DeInit(void);
DYP_StatusTypeDef DYP_StartMeasurement(void);
void DYP_Process(void);
DYP_StateTypeDef DYP_GetState(void);
DYP_StatusTypeDef DYP_GetLastMeasurement(uint16_t *distance_mm,
                                         uint32_t *age_ms);
DYP_StatusTypeDef DYP_GetSnapshot(DYP_SnapshotTypeDef *snapshot);
uint8_t DYP_IsReady(void);
uint8_t DYP_IsBusy(void);
uint32_t DYP_GetLastHalError(void);

void DYP_UART_IRQHandler(void);
void DYP_UART_RxCpltCallback(UART_HandleTypeDef *uart);
void DYP_UART_ErrorCallback(UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif /* DYP_H */
