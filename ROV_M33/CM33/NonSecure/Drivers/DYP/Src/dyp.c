/**
  ******************************************************************************
  * @file    dyp.c
  * @brief   Non-blocking DYP-L08 UART controlled-output driver.
  ******************************************************************************
  */

#include "dyp.h"

#include "res_mgr.h"

#include <string.h>

#define DYP_UART_RIF_RESOURCE        STM32MP25_RIFSC_UART4_ID
#define DYP_RX_GPIO_PIN_NUMBER       6U
#define DYP_TRIGGER_GPIO_PIN_NUMBER  7U
/* Unified RCC clock resource id of GPIOB (96 is GPIOG; verified against the
 * STM32MP25 RIF resource tables). */
#define DYP_GPIO_RCC_RESOURCE        91U
#define DYP_RESOURCE_ERROR_BASE      0xD9000000UL

typedef struct
{
    UART_HandleTypeDef uart;
    volatile DYP_StateTypeDef state;
    volatile uint8_t rx_complete;
    volatile uint8_t uart_error;
    uint8_t frame[DYP_FRAME_SIZE];
    uint16_t distance_mm;
    uint32_t trigger_tick;
    uint32_t last_valid_tick;
    uint32_t last_hal_error;
    uint8_t initialized;
    uint8_t valid;
    uint8_t trigger_high_pending;
    uint8_t has_triggered;
    uint8_t uart_resource_acquired;
    uint8_t rx_gpio_resource_acquired;
    uint8_t trigger_gpio_resource_acquired;
    uint8_t gpio_rcc_resource_acquired;
    uint8_t uart_clock_enabled;
    uint8_t uart_hal_initialized;
    uint8_t rx_gpio_configured;
    uint8_t trigger_gpio_configured;
    uint8_t irq_enabled;
} DYP_DeviceTypeDef;

static DYP_DeviceTypeDef dyp;

static uint8_t DYP_RequestResource(ResMgr_Res_Type_t type,
                                   uint8_t resource,
                                   uint8_t *ownership,
                                   uint32_t error_code)
{
    if (ResMgr_Request(type, resource) != RESMGR_STATUS_ACCESS_OK)
    {
        dyp.last_hal_error = DYP_RESOURCE_ERROR_BASE | error_code;
        return 0U;
    }

    *ownership = 1U;
    return 1U;
}

static void DYP_SetTriggerHigh(void)
{
    if (dyp.trigger_gpio_configured != 0U)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    }
    dyp.trigger_high_pending = 0U;
}

static void DYP_AbortReceive(void)
{
    /* IT reception has no DMA in this driver and normally aborts immediately.
     * Keep the synchronous API as a fallback if HAL cannot start the IT abort. */
    if (HAL_UART_AbortReceive_IT(&dyp.uart) != HAL_OK)
    {
        (void)HAL_UART_AbortReceive(&dyp.uart);
    }

    dyp.rx_complete = 0U;
    dyp.uart_error = 0U;
    memset(dyp.frame, 0, sizeof(dyp.frame));
}

static void DYP_ReleaseResources(void)
{
    if (dyp.trigger_gpio_configured != 0U)
    {
        DYP_SetTriggerHigh();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
        dyp.trigger_gpio_configured = 0U;
    }
    if (dyp.rx_gpio_configured != 0U)
    {
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);
        dyp.rx_gpio_configured = 0U;
    }
    if (dyp.uart_clock_enabled != 0U)
    {
        __HAL_RCC_UART4_CLK_DISABLE();
        dyp.uart_clock_enabled = 0U;
    }
    if (dyp.gpio_rcc_resource_acquired != 0U)
    {
        __HAL_RCC_GPIOB_CLK_DISABLE();
        (void)ResMgr_Release(RESMGR_RESOURCE_RIF_RCC,
                             RESMGR_RCC_RESOURCE(DYP_GPIO_RCC_RESOURCE));
        dyp.gpio_rcc_resource_acquired = 0U;
    }
    if (dyp.trigger_gpio_resource_acquired != 0U)
    {
        (void)ResMgr_Release(RESMGR_RESOURCE_RIF_GPIOB,
                             RESMGR_GPIO_PIN(DYP_TRIGGER_GPIO_PIN_NUMBER));
        dyp.trigger_gpio_resource_acquired = 0U;
    }
    if (dyp.rx_gpio_resource_acquired != 0U)
    {
        (void)ResMgr_Release(RESMGR_RESOURCE_RIF_GPIOB,
                             RESMGR_GPIO_PIN(DYP_RX_GPIO_PIN_NUMBER));
        dyp.rx_gpio_resource_acquired = 0U;
    }
    if (dyp.uart_resource_acquired != 0U)
    {
        (void)ResMgr_Release(RESMGR_RESOURCE_RIFSC,
                             DYP_UART_RIF_RESOURCE);
        dyp.uart_resource_acquired = 0U;
    }
}

DYP_StatusTypeDef DYP_Init(void)
{
    GPIO_InitTypeDef gpio;
    HAL_StatusTypeDef hal_status;

    memset(&dyp, 0, sizeof(dyp));
    dyp.state = DYP_STATE_UNINITIALIZED;

    if (DYP_RequestResource(RESMGR_RESOURCE_RIFSC,
                            DYP_UART_RIF_RESOURCE,
                            &dyp.uart_resource_acquired, 1U) == 0U)
    {
        DYP_ReleaseResources();
        return DYP_STATUS_NOT_READY;
    }
    if (DYP_RequestResource(RESMGR_RESOURCE_RIF_GPIOB,
                            RESMGR_GPIO_PIN(DYP_RX_GPIO_PIN_NUMBER),
                            &dyp.rx_gpio_resource_acquired, 2U) == 0U)
    {
        DYP_ReleaseResources();
        return DYP_STATUS_NOT_READY;
    }
    if (DYP_RequestResource(RESMGR_RESOURCE_RIF_GPIOB,
                            RESMGR_GPIO_PIN(DYP_TRIGGER_GPIO_PIN_NUMBER),
                            &dyp.trigger_gpio_resource_acquired, 3U) == 0U)
    {
        DYP_ReleaseResources();
        return DYP_STATUS_NOT_READY;
    }

    if (__HAL_RCC_GPIOB_IS_CLK_ENABLED() == 0U)
    {
        if (DYP_RequestResource(RESMGR_RESOURCE_RIF_RCC,
                                RESMGR_RCC_RESOURCE(DYP_GPIO_RCC_RESOURCE),
                                &dyp.gpio_rcc_resource_acquired, 4U) == 0U)
        {
            DYP_ReleaseResources();
            return DYP_STATUS_NOT_READY;
        }
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }

    __HAL_RCC_UART4_CLK_ENABLE();
    dyp.uart_clock_enabled = 1U;

    /* Set the output latch before changing PB7 to output mode. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    memset(&gpio, 0, sizeof(gpio));
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    dyp.trigger_gpio_configured = 1U;

    memset(&gpio, 0, sizeof(gpio));
    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF3_UART4;
    HAL_GPIO_Init(GPIOB, &gpio);
    dyp.rx_gpio_configured = 1U;

    memset(&dyp.uart, 0, sizeof(dyp.uart));
    dyp.uart.Instance = UART4;
    dyp.uart.Init.BaudRate = 115200U;
    dyp.uart.Init.WordLength = UART_WORDLENGTH_8B;
    dyp.uart.Init.StopBits = UART_STOPBITS_1;
    dyp.uart.Init.Parity = UART_PARITY_NONE;
    dyp.uart.Init.Mode = UART_MODE_RX;
    dyp.uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    dyp.uart.Init.OverSampling = UART_OVERSAMPLING_8;
    dyp.uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    dyp.uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    dyp.uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    hal_status = HAL_UART_Init(&dyp.uart);
    if (hal_status != HAL_OK)
    {
        dyp.last_hal_error = HAL_UART_GetError(&dyp.uart);
        /* HAL init can fail after partially touching the peripheral. */
        (void)HAL_UART_DeInit(&dyp.uart);
        DYP_ReleaseResources();
        return DYP_STATUS_IO_ERROR;
    }
    dyp.uart_hal_initialized = 1U;

    HAL_NVIC_SetPriority(UART4_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    dyp.irq_enabled = 1U;

    dyp.initialized = 1U;
    dyp.state = DYP_STATE_IDLE;
    return DYP_STATUS_OK;
}

void DYP_DeInit(void)
{
    if (dyp.trigger_high_pending != 0U)
    {
        DYP_SetTriggerHigh();
    }
    if ((dyp.initialized != 0U) && (dyp.state == DYP_STATE_WAITING))
    {
        (void)HAL_UART_AbortReceive_IT(&dyp.uart);
    }
    if (dyp.irq_enabled != 0U)
    {
        HAL_NVIC_DisableIRQ(UART4_IRQn);
        dyp.irq_enabled = 0U;
    }
    if (dyp.uart_hal_initialized != 0U)
    {
        (void)HAL_UART_DeInit(&dyp.uart);
        dyp.uart_hal_initialized = 0U;
    }

    DYP_ReleaseResources();
    dyp.initialized = 0U;
    dyp.valid = 0U;
    dyp.rx_complete = 0U;
    dyp.uart_error = 0U;
    dyp.state = DYP_STATE_UNINITIALIZED;
}

DYP_StatusTypeDef DYP_StartMeasurement(void)
{
    HAL_StatusTypeDef hal_status;
    uint32_t now;

    if (dyp.initialized == 0U)
    {
        return DYP_STATUS_NOT_READY;
    }
    if (dyp.state == DYP_STATE_WAITING)
    {
        return DYP_STATUS_BUSY;
    }

    now = HAL_GetTick();
    if ((dyp.has_triggered != 0U) &&
        ((uint32_t)(now - dyp.trigger_tick) < DYP_TRIGGER_MIN_INTERVAL_MS))
    {
        return DYP_STATUS_BUSY;
    }

    dyp.rx_complete = 0U;
    dyp.uart_error = 0U;
    memset(dyp.frame, 0, sizeof(dyp.frame));
    hal_status = HAL_UART_Receive_IT(&dyp.uart, dyp.frame, DYP_FRAME_SIZE);
    if (hal_status == HAL_BUSY)
    {
        return DYP_STATUS_BUSY;
    }
    if (hal_status != HAL_OK)
    {
        dyp.last_hal_error = HAL_UART_GetError(&dyp.uart);
        dyp.state = DYP_STATE_IO_ERROR;
        return DYP_STATUS_IO_ERROR;
    }

    dyp.has_triggered = 1U;
    dyp.trigger_high_pending = 1U;
    dyp.state = DYP_STATE_WAITING;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    /* Start timing after PB7 is physically commanded LOW. */
    dyp.trigger_tick = HAL_GetTick();
    return DYP_STATUS_OK;
}

void DYP_Process(void)
{
    uint32_t now;
    uint8_t checksum;

    if ((dyp.initialized == 0U) || (dyp.state != DYP_STATE_WAITING))
    {
        return;
    }

    now = HAL_GetTick();
    if ((dyp.trigger_high_pending != 0U) &&
        ((uint32_t)(now - dyp.trigger_tick) >= DYP_TRIGGER_PULSE_MS))
    {
        DYP_SetTriggerHigh();
    }

    if (dyp.uart_error != 0U)
    {
        DYP_SetTriggerHigh();
        DYP_AbortReceive();
        dyp.state = DYP_STATE_IO_ERROR;
        return;
    }

    if (dyp.rx_complete != 0U)
    {
        DYP_SetTriggerHigh();
        checksum = (uint8_t)(dyp.frame[0] + dyp.frame[1] + dyp.frame[2]);
        if ((dyp.frame[0] != 0xFFU) || (dyp.frame[3] != checksum))
        {
            dyp.state = DYP_STATE_IO_ERROR;
            return;
        }

        dyp.distance_mm = (uint16_t)(((uint16_t)dyp.frame[1] << 8) |
                                     (uint16_t)dyp.frame[2]);
        dyp.last_valid_tick = now;
        dyp.valid = 1U;
        dyp.state = DYP_STATE_COMPLETE;
        return;
    }

    if ((uint32_t)(now - dyp.trigger_tick) >= DYP_RESPONSE_TIMEOUT_MS)
    {
        DYP_SetTriggerHigh();
        DYP_AbortReceive();
        dyp.state = DYP_STATE_TIMEOUT;
    }
}

DYP_StateTypeDef DYP_GetState(void)
{
    return dyp.state;
}

DYP_StatusTypeDef DYP_GetLastMeasurement(uint16_t *distance_mm,
                                         uint32_t *age_ms)
{
    if ((distance_mm == NULL) || (age_ms == NULL))
    {
        return DYP_STATUS_BAD_ARG;
    }
    if (dyp.initialized == 0U)
    {
        return DYP_STATUS_NOT_READY;
    }
    if (dyp.valid == 0U)
    {
        return DYP_STATUS_NOT_READY;
    }

    *distance_mm = dyp.distance_mm;
    *age_ms = HAL_GetTick() - dyp.last_valid_tick;
    return DYP_STATUS_OK;
}

DYP_StatusTypeDef DYP_GetSnapshot(DYP_SnapshotTypeDef *snapshot)
{
    if (snapshot == NULL)
    {
        return DYP_STATUS_BAD_ARG;
    }

    snapshot->state = dyp.state;
    snapshot->distance_mm = dyp.distance_mm;
    snapshot->age_ms = (dyp.valid != 0U) ?
                       (HAL_GetTick() - dyp.last_valid_tick) : 0U;
    snapshot->last_hal_error = dyp.last_hal_error;
    snapshot->ready = dyp.initialized;
    snapshot->busy = (dyp.state == DYP_STATE_WAITING) ? 1U : 0U;
    snapshot->valid = dyp.valid;
    return DYP_STATUS_OK;
}

uint8_t DYP_IsReady(void)
{
    return dyp.initialized;
}

uint8_t DYP_IsBusy(void)
{
    return (dyp.state == DYP_STATE_WAITING) ? 1U : 0U;
}

uint32_t DYP_GetLastHalError(void)
{
    return dyp.last_hal_error;
}

void DYP_UART_IRQHandler(void)
{
    if (dyp.initialized != 0U)
    {
        HAL_UART_IRQHandler(&dyp.uart);
    }
}

void DYP_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if ((uart != NULL) && (uart->Instance == UART4) &&
        (uart == &dyp.uart) && (dyp.state == DYP_STATE_WAITING))
    {
        dyp.rx_complete = 1U;
    }
}

void DYP_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if ((uart != NULL) && (uart->Instance == UART4) &&
        (uart == &dyp.uart) && (dyp.state == DYP_STATE_WAITING))
    {
        dyp.last_hal_error = HAL_UART_GetError(uart);
        dyp.uart_error = 1U;
    }
}
