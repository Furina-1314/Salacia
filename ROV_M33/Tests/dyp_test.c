#include "dyp.h"
#include "res_mgr.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

USART_TypeDef test_uart4_instance;
GPIO_TypeDef test_gpiob_instance;

static uint32_t test_tick;
static uint8_t gpio_clock_enabled;
static uint8_t uart_clock_enabled;
static uint8_t trigger_level;
static uint8_t *rx_buffer;
static uint16_t rx_size;
static UART_HandleTypeDef *rx_uart;
static HAL_StatusTypeDef next_uart_init_status;
static HAL_StatusTypeDef next_receive_status;
static HAL_StatusTypeDef next_abort_it_status;
static HAL_StatusTypeDef next_abort_status;
static uint32_t uart_error;
static uint8_t rx_active;
static unsigned int request_count;
static unsigned int release_count;
static unsigned int request_fail_at;
static unsigned int abort_count;
static unsigned int blocking_abort_count;
static unsigned int deinit_count;
static unsigned int irq_enable_count;
static unsigned int irq_disable_count;
static GPIO_InitTypeDef pb6_config;
static GPIO_InitTypeDef pb7_config;
static char event_log[32];
static size_t event_count;

static void log_event(char event)
{
    if (event_count < (sizeof(event_log) - 1U))
    {
        event_log[event_count++] = event;
        event_log[event_count] = '\0';
    }
}

static void reset_mocks(void)
{
    test_tick = 0U;
    gpio_clock_enabled = 0U;
    uart_clock_enabled = 0U;
    trigger_level = GPIO_PIN_RESET;
    rx_buffer = NULL;
    rx_size = 0U;
    rx_uart = NULL;
    next_uart_init_status = HAL_OK;
    next_receive_status = HAL_OK;
    next_abort_it_status = HAL_OK;
    next_abort_status = HAL_OK;
    uart_error = 0U;
    rx_active = 0U;
    request_count = 0U;
    release_count = 0U;
    request_fail_at = 0U;
    abort_count = 0U;
    blocking_abort_count = 0U;
    deinit_count = 0U;
    irq_enable_count = 0U;
    irq_disable_count = 0U;
    memset(&pb6_config, 0, sizeof(pb6_config));
    memset(&pb7_config, 0, sizeof(pb7_config));
    memset(event_log, 0, sizeof(event_log));
    event_count = 0U;
}

ResMgr_Status_t ResMgr_Request(ResMgr_Res_Type_t type, uint8_t resource)
{
    (void)type;
    (void)resource;
    ++request_count;
    if ((request_fail_at != 0U) && (request_count == request_fail_at))
    {
        return RESMGR_STATUS_ACCESS_ERROR;
    }
    return RESMGR_STATUS_ACCESS_OK;
}

ResMgr_Status_t ResMgr_Release(ResMgr_Res_Type_t type, uint8_t resource)
{
    (void)type;
    (void)resource;
    ++release_count;
    return RESMGR_STATUS_ACCESS_OK;
}

uint8_t Test_HAL_RCC_GPIOB_IsEnabled(void)
{
    return gpio_clock_enabled;
}

void Test_HAL_RCC_GPIOB_Enable(void)
{
    gpio_clock_enabled = 1U;
}

void Test_HAL_RCC_GPIOB_Disable(void)
{
    gpio_clock_enabled = 0U;
}

void Test_HAL_RCC_UART4_Enable(void)
{
    uart_clock_enabled = 1U;
}

void Test_HAL_RCC_UART4_Disable(void)
{
    uart_clock_enabled = 0U;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, uint32_t state)
{
    assert(port == GPIOB);
    if (pin == GPIO_PIN_7)
    {
        trigger_level = (uint8_t)state;
        log_event((state == GPIO_PIN_SET) ? 'H' : 'L');
    }
}

void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *init)
{
    assert(port == GPIOB);
    if (init->Pin == GPIO_PIN_7)
    {
        pb7_config = *init;
        log_event('7');
    }
    else if (init->Pin == GPIO_PIN_6)
    {
        pb6_config = *init;
        log_event('6');
    }
}

void HAL_GPIO_DeInit(GPIO_TypeDef *port, uint32_t pin)
{
    (void)port;
    (void)pin;
}

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *uart)
{
    assert(uart->Instance == UART4);
    return next_uart_init_status;
}

HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef *uart)
{
    assert(uart->Instance == UART4);
    ++deinit_count;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,
                                      uint8_t *data,
                                      uint16_t size)
{
    if (rx_active != 0U)
    {
        return HAL_BUSY;
    }
    if (next_receive_status == HAL_OK)
    {
        rx_uart = uart;
        rx_buffer = data;
        rx_size = size;
        rx_active = 1U;
    }
    return next_receive_status;
}

HAL_StatusTypeDef HAL_UART_AbortReceive_IT(UART_HandleTypeDef *uart)
{
    (void)uart;
    ++abort_count;
    if (next_abort_it_status == HAL_OK)
    {
        rx_active = 0U;
    }
    return next_abort_it_status;
}

HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart)
{
    (void)uart;
    ++blocking_abort_count;
    if (next_abort_status == HAL_OK)
    {
        rx_active = 0U;
    }
    return next_abort_status;
}

void HAL_UART_IRQHandler(UART_HandleTypeDef *uart)
{
    (void)uart;
}

uint32_t HAL_UART_GetError(UART_HandleTypeDef *uart)
{
    (void)uart;
    return uart_error;
}

uint32_t HAL_GetTick(void)
{
    return test_tick;
}

void HAL_NVIC_SetPriority(int32_t irq, uint32_t priority,
                          uint32_t subpriority)
{
    assert(irq == UART4_IRQn);
    assert(priority == 1U);
    assert(subpriority == 0U);
}

void HAL_NVIC_EnableIRQ(int32_t irq)
{
    assert(irq == UART4_IRQn);
    ++irq_enable_count;
}

void HAL_NVIC_DisableIRQ(int32_t irq)
{
    assert(irq == UART4_IRQn);
    ++irq_disable_count;
}

static void complete_frame(uint8_t header, uint8_t high, uint8_t low,
                           uint8_t checksum)
{
    assert(rx_buffer != NULL);
    assert(rx_size == DYP_FRAME_SIZE);
    rx_buffer[0] = header;
    rx_buffer[1] = high;
    rx_buffer[2] = low;
    rx_buffer[3] = checksum;
    rx_active = 0U;
    DYP_UART_RxCpltCallback(rx_uart);
    DYP_Process();
}

static void test_init_and_normal_frame(void)
{
    uint16_t distance;
    uint32_t age;

    reset_mocks();
    assert(DYP_Init() == DYP_STATUS_OK);
    assert(request_count == 4U);
    assert(strcmp(event_log, "H76") == 0);
    assert(trigger_level == GPIO_PIN_SET);
    assert(pb7_config.Mode == GPIO_MODE_OUTPUT_PP);
    assert(pb6_config.Mode == GPIO_MODE_AF_PP);
    assert(pb6_config.Alternate == GPIO_AF3_UART4);
    assert(uart_clock_enabled == 1U);
    assert(irq_enable_count == 1U);

    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    assert(DYP_GetState() == DYP_STATE_WAITING);
    assert(trigger_level == GPIO_PIN_RESET);
    test_tick = DYP_TRIGGER_PULSE_MS;
    DYP_Process();
    assert(trigger_level == GPIO_PIN_SET);

    test_tick = 18U;
    complete_frame(0xFFU, 0x01U, 0x10U, 0x10U);
    assert(DYP_GetState() == DYP_STATE_COMPLETE);
    assert(DYP_GetLastMeasurement(&distance, &age) == DYP_STATUS_OK);
    assert(distance == 272U);
    assert(age == 0U);

    test_tick = 20U;
    assert(DYP_GetLastMeasurement(&distance, &age) == DYP_STATUS_OK);
    assert(age == 2U);
    DYP_DeInit();
    assert(irq_disable_count == 1U);
    assert(uart_clock_enabled == 0U);
    assert(release_count == 4U);
}

static void test_bad_frames(void)
{
    reset_mocks();
    assert(DYP_Init() == DYP_STATUS_OK);
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    test_tick = 18U;
    complete_frame(0x00U, 0x01U, 0x10U, 0x11U);
    assert(DYP_GetState() == DYP_STATE_IO_ERROR);

    test_tick = DYP_TRIGGER_MIN_INTERVAL_MS;
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    test_tick += 18U;
    complete_frame(0xFFU, 0x01U, 0x10U, 0x11U);
    assert(DYP_GetState() == DYP_STATE_IO_ERROR);
    DYP_DeInit();
}

static void test_busy_timeout_and_recovery(void)
{
    uint16_t distance;
    uint32_t age;

    reset_mocks();
    assert(DYP_StartMeasurement() == DYP_STATUS_NOT_READY);
    assert(DYP_Init() == DYP_STATUS_OK);
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    assert(DYP_StartMeasurement() == DYP_STATUS_BUSY);

    test_tick = DYP_RESPONSE_TIMEOUT_MS;
    DYP_Process();
    assert(DYP_GetState() == DYP_STATE_TIMEOUT);
    assert(abort_count == 1U);

    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    test_tick += 18U;
    complete_frame(0xFFU, 0x01U, 0x10U, 0x10U);
    assert(DYP_GetLastMeasurement(&distance, &age) == DYP_STATUS_OK);
    assert(distance == 272U);

    DYP_DeInit();
}

static void test_uart_error_abort_and_recovery(void)
{
    reset_mocks();
    assert(DYP_Init() == DYP_STATUS_OK);
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    assert(rx_active == 1U);

    uart_error = 0x55U;
    DYP_UART_ErrorCallback(rx_uart);
    assert(rx_active == 1U);
    DYP_Process();
    assert(trigger_level == GPIO_PIN_SET);
    assert(DYP_GetState() == DYP_STATE_IO_ERROR);
    assert(DYP_GetLastHalError() == 0x55U);
    assert(abort_count == 1U);
    assert(rx_active == 0U);

    test_tick = DYP_TRIGGER_MIN_INTERVAL_MS;
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    assert(DYP_GetState() == DYP_STATE_WAITING);

    /* If the preferred IT abort fails, the synchronous HAL cleanup prevents
     * the previous receive state from leaving the next request HAL_BUSY. */
    uart_error = 0x66U;
    next_abort_it_status = HAL_ERROR;
    DYP_UART_ErrorCallback(rx_uart);
    DYP_Process();
    assert(DYP_GetState() == DYP_STATE_IO_ERROR);
    assert(abort_count == 2U);
    assert(blocking_abort_count == 1U);
    assert(rx_active == 0U);

    test_tick += DYP_TRIGGER_MIN_INTERVAL_MS;
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    assert(DYP_GetState() == DYP_STATE_WAITING);
    DYP_DeInit();
}

static void test_min_interval_and_failure_cleanup(void)
{
    reset_mocks();
    assert(DYP_Init() == DYP_STATUS_OK);
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    test_tick = 18U;
    complete_frame(0xFFU, 0x00U, 0x64U, 0x63U);
    test_tick = DYP_TRIGGER_MIN_INTERVAL_MS - 1U;
    assert(DYP_StartMeasurement() == DYP_STATUS_BUSY);
    test_tick = DYP_TRIGGER_MIN_INTERVAL_MS;
    assert(DYP_StartMeasurement() == DYP_STATUS_OK);
    DYP_DeInit();

    reset_mocks();
    request_fail_at = 3U;
    assert(DYP_Init() == DYP_STATUS_NOT_READY);
    assert(release_count == 2U);
    assert(DYP_IsReady() == 0U);

    reset_mocks();
    next_uart_init_status = HAL_ERROR;
    uart_error = 0x66U;
    assert(DYP_Init() == DYP_STATUS_IO_ERROR);
    assert(DYP_GetLastHalError() == 0x66U);
    assert(deinit_count == 1U);
    assert(release_count == 4U);
    assert(uart_clock_enabled == 0U);
    assert(gpio_clock_enabled == 0U);
    assert(DYP_IsReady() == 0U);
}

int main(void)
{
    test_init_and_normal_frame();
    test_bad_frames();
    test_busy_timeout_and_recovery();
    test_uart_error_abort_and_recovery();
    test_min_interval_and_failure_cleanup();
    puts("dyp_test: PASS");
    return 0;
}
