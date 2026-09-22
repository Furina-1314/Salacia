/**
  ******************************************************************************
  * @file    main.c
  * @author  MCD Application Team / Modified for ROV actuator control
  * @brief   Main program body
  ******************************************************************************
  */

#include "main.h"
#include "actuator_service.h"
#include "attitude_estimator.h"
#include "command_service.h"
#include "copro_sync.h"
#include "dyp.h"
#include "propeller_service.h"
#include "res_mgr.h"
#include "sensor_service.h"
#include "stability_control.h"

#include <string.h>

/* Private define ------------------------------------------------------------ */

#define MAX_BUFFER_SIZE          RPMSG_BUFFER_SIZE
#define BUS_I2Cx_FREQUENCY       0x2050606FU
#define I2C8_SCL_PIN             4U
#define I2C8_SDA_PIN             9U
#define I2C8_RCC_RESOURCE        101U
#define SENSOR_PERIOD_MS          10U
#define CONTROL_PERIOD_MS         20U
#define ATTITUDE_FRESH_MS         30U
#define ATTITUDE_MAX_DT_MS        50U

/* Private variables --------------------------------------------------------- */

IPCC_HandleTypeDef   hipcc;
I2C_HandleTypeDef    I2cHandle;
I2C_HandleTypeDef    I2c8Handle;

/* USER CODE BEGIN PV */

VIRT_UART_HandleTypeDef huart0;
static uint8_t I2c4ClockResourceAcquired = 0U;
static uint8_t I2c8RccResourceAcquired = 0U;
static uint8_t I2c8BusClockResourceAcquired = 0U;
static uint8_t I2c8HalInitialized = 0U;

__IO FlagStatus VirtUart0RxMsg = RESET;

/*
 * RPMsg command queue: produced in the IPCC RX interrupt, consumed by the
 * main loop. Command handlers issue blocking actuator I2C transfers; running
 * them in the interrupt froze the HAL tick (SysTick priority is lower than
 * IPCC) so an I2C stall could never time out and wedged the whole core.
 */
#define COMMAND_QUEUE_DEPTH 16U
#define COMMAND_MAX_LEN     63U

static char CommandQueue[COMMAND_QUEUE_DEPTH][COMMAND_MAX_LEN + 1U];
static volatile uint8_t CommandQueueHead = 0U; /* producer (ISR) */
static volatile uint8_t CommandQueueTail = 0U; /* consumer (main loop) */

uint8_t VirtUart0ChannelBuffTx[MAX_BUFFER_SIZE];
uint8_t VirtUart0ChannelBuffRx[MAX_BUFFER_SIZE];

uint16_t VirtUart0ChannelTxSize = 0;
uint16_t VirtUart0ChannelRxSize = 0;

/* USER CODE END PV */

/* Private function prototypes ----------------------------------------------- */

static void MX_IPCC_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C4_Init(void);
static void MX_I2C4_DeInit(void);
static uint8_t MX_I2C8_Init(void);
static void MX_I2C8_DeInit(void);

void HAL_I2C_MspPostInit(I2C_HandleTypeDef *hi2c);

static void SendReply(const char *msg);
static void ReportActuatorIoError(uint32_t hal_error);

void VIRT_UART0_RxCpltCallback(VIRT_UART_HandleTypeDef *huart);

/**
  * @brief  Main program
  * @param  None
  * @retval int
  */
int main(void)
{
    SensorService_StatusTypeDef sensor_status;
    ActuatorService_StatusTypeDef actuator_status;
    PropellerService_StatusTypeDef propeller_status =
        PROPELLER_SERVICE_STATUS_NOT_READY;
    AttitudeEstimator_ConfigTypeDef attitude_config = {
        ATTITUDE_ESTIMATOR_DEFAULT_ALPHA,
        ATTITUDE_ESTIMATOR_GYRO_LSB_PER_DPS,
        ATTITUDE_ESTIMATOR_DEFAULT_CALIBRATION_SAMPLES
    };
    AttitudeEstimator_StateTypeDef attitude_state;
    PropellerService_StateTypeDef propeller_state;
    MPU6500_RawData mpu_sample;
    uint32_t last_sensor_tick;
    uint32_t last_control_tick;
    uint32_t last_attitude_tick = 0U;
    uint32_t now;
    uint32_t elapsed;
    float sensor_dt;
    uint8_t attitude_fresh;

    HAL_Init();

    loc_printf("\r\n");
    loc_printf("Starting ROV actuator control application\r\n");
    loc_printf("I2C4: PD10=SDA, PD11=SCL\r\n");

    /*
     * IPCC initialization.
     */
    MX_IPCC_Init();

    if (!IS_DEVELOPER_BOOT_MODE())
    {
        CoproSync_Init();
    }

    /*
     * Update system clock variable.
     */
    SystemCoreClockUpdate();

    /*
     * Establish the diagnostic/control channel before optional business
     * services. OpenAMP/IPCC failures are core startup failures.
     */
    if (MX_OPENAMP_Init(RPMSG_REMOTE, NULL) != 0)
    {
        Error_Handler();
    }

    loc_printf("Virtual UART0 OpenAMP-rpmsg channel creation\r\n");

    if (VIRT_UART_Init(&huart0) != VIRT_UART_OK)
    {
        loc_printf("VIRT_UART_Init UART0 failed.\r\n");
        Error_Handler();
    }

    CommandService_Init();

    if (VIRT_UART_RegisterCallback(&huart0,
                                   VIRT_UART_RXCPLT_CB_ID,
                                   VIRT_UART0_RxCpltCallback)
        != VIRT_UART_OK)
    {
        loc_printf("VIRT_UART_RegisterCallback failed.\r\n");
        Error_Handler();
    }

    /* DYP is optional: failure leaves RPMsg and all other services available. */
    (void)SensorService_InitDyp();

    /*
     * Keep the verified I2C4 path unchanged.
     */
    MX_DMA_Init();
    MX_I2C4_Init();

    actuator_status = ActuatorService_Init(&I2cHandle);
    if (actuator_status == ACTUATOR_SERVICE_STATUS_OK)
    {
        propeller_status = PropellerService_Init();
        if (propeller_status != PROPELLER_SERVICE_STATUS_OK)
        {
            loc_printf("Propeller unavailable: status=%u\r\n",
                       (unsigned int)propeller_status);
        }
    }
    else
    {
        loc_printf("Actuator unavailable: status=%u HAL error=0x%08lX\r\n",
                   (unsigned int)actuator_status,
                   (unsigned long)ActuatorService_GetLastHalError());
    }

    sensor_status = SENSOR_SERVICE_STATUS_NOT_READY;
    if (MX_I2C8_Init() != 0U)
    {
        sensor_status = SensorService_Init(&I2c8Handle);
    }
    if (sensor_status == SENSOR_SERVICE_STATUS_OK)
    {
        loc_printf("MPU6500 ready: WHO_AM_I=0x%02X\r\n",
                   SensorService_GetMpuWhoAmI());
    }
    else
    {
        /*
         * The sensor is optional at startup. Keep the actuator and
         * OpenAMP baseline available and report err not_ready to sensor mpu.
         */
        loc_printf("MPU6500 not ready: status=%u HAL error=0x%08lX\r\n",
                   (unsigned int)sensor_status,
                   (unsigned long)SensorService_GetMpuLastHalError());
    }

    if (AttitudeEstimator_Init(&attitude_config) !=
        ATTITUDE_ESTIMATOR_STATUS_OK)
    {
        loc_printf("AttitudeEstimator unavailable\r\n");
    }
    if (StabilityControl_Init() != STABILITY_CONTROL_STATUS_OK)
    {
        loc_printf("StabilityControl unavailable\r\n");
    }

    if ((actuator_status == ACTUATOR_SERVICE_STATUS_OK) &&
        (propeller_status == PROPELLER_SERVICE_STATUS_OK))
    {
        SendReply("ACTUATOR I2C4 READY\r\n");
    }
    else
    {
        SendReply("M33 RPMSG READY; ACTUATOR NOT READY\r\n");
    }

    now = HAL_GetTick();
    last_sensor_tick = now;
    last_control_tick = now;
    if (PropellerService_GetState(&propeller_state) ==
        PROPELLER_SERVICE_STATUS_OK)
    {
        (void)StabilityControl_SyncState(&propeller_state);
    }

    /*
     * Main loop.
     */
    while (1)
    {
        OPENAMP_check_for_message();

        /*
         * Dispatch queued RPMsg commands. Handlers may block briefly on
         * actuator I2C; with SysTick running here their HAL timeouts are
         * real, so a wedged bus degrades to "err io" instead of a locked
         * core.
         */
        while (CommandQueueTail != CommandQueueHead)
        {
            CommandService_Handle(CommandQueue[CommandQueueTail],
                                  SendReply,
                                  ReportActuatorIoError);
            CommandQueueTail = (uint8_t)((CommandQueueTail + 1U) &
                                         (COMMAND_QUEUE_DEPTH - 1U));
            OPENAMP_check_for_message();
        }

        SensorService_Process();
        CommandService_Process();

        if (PropellerService_GetState(&propeller_state) ==
            PROPELLER_SERVICE_STATUS_OK)
        {
            (void)StabilityControl_SyncState(&propeller_state);
        }

        now = HAL_GetTick();
        elapsed = now - last_sensor_tick;
        if (elapsed >= SENSOR_PERIOD_MS)
        {
            last_sensor_tick = now;
            if (SensorService_ReadMpuRaw(&mpu_sample) ==
                SENSOR_SERVICE_STATUS_OK)
            {
                if (AttitudeEstimator_IsReady() == 0U)
                {
                    sensor_dt = (float)SENSOR_PERIOD_MS / 1000.0f;
                }
                else if (elapsed <= ATTITUDE_MAX_DT_MS)
                {
                    sensor_dt = (float)elapsed / 1000.0f;
                }
                else
                {
                    sensor_dt = (float)SENSOR_PERIOD_MS / 1000.0f;
                }

                if ((AttitudeEstimator_Update(&mpu_sample, sensor_dt) ==
                     ATTITUDE_ESTIMATOR_STATUS_OK) ||
                    (AttitudeEstimator_IsReady() == 0U))
                {
                    last_attitude_tick = now;
                }
            }
        }

        OPENAMP_check_for_message();

        now = HAL_GetTick();
        elapsed = now - last_control_tick;
        if (elapsed >= CONTROL_PERIOD_MS)
        {
            last_control_tick = now;
            if ((PropellerService_GetState(&propeller_state) ==
                 PROPELLER_SERVICE_STATUS_OK) &&
                (AttitudeEstimator_GetState(&attitude_state) ==
                 ATTITUDE_ESTIMATOR_STATUS_OK))
            {
                attitude_fresh = (attitude_state.ready != 0U) &&
                    ((uint32_t)(now - last_attitude_tick) <=
                     ATTITUDE_FRESH_MS);
                (void)StabilityControl_Execute(
                    attitude_state.roll_deg,
                    attitude_state.pitch_deg,
                    attitude_state.ready,
                    attitude_fresh,
                    &propeller_state,
                    (float)CONTROL_PERIOD_MS / 1000.0f);
            }
        }

        OPENAMP_check_for_message();

        if (VirtUart0RxMsg)
        {
            VirtUart0RxMsg = RESET;
        }
    }
}

/**
  * @brief  Send a string through RPMsg Virtual UART.
  * @param  msg Null-terminated string
  * @retval None
  */
static void SendReply(const char *msg)
{
    uint16_t len;
    uint8_t attempts;

    if (msg == NULL)
    {
        return;
    }

    len = (uint16_t)strlen(msg);

    if (len >= MAX_BUFFER_SIZE)
    {
        len = MAX_BUFFER_SIZE - 1U;
    }

    memcpy(VirtUart0ChannelBuffTx,
           msg,
           len);

    VirtUart0ChannelTxSize = len;

    /* OPENAMP_send fails while the M33 tx buffer pool is exhausted (request
     * bursts from the A35 gateway). The IPCC virtqueue interrupt returns
     * buffers asynchronously; without a retry the reply is silently dropped
     * and the A35 client reports a request deadline. Retry bounded so the
     * main loop keeps its cadence even if buffers never return. */
    attempts = 0U;
    while (VIRT_UART_Transmit(&huart0,
                              VirtUart0ChannelBuffTx,
                              VirtUart0ChannelTxSize) != VIRT_UART_OK)
    {
        if (attempts >= 50U)
        {
            break;
        }
        ++attempts;
        OPENAMP_check_for_message();
        HAL_Delay(1U);
    }
}

static void ReportActuatorIoError(uint32_t hal_error)
{
    loc_printf("Actuator I2C error: HAL error=0x%08lX\r\n",
               (unsigned long)hal_error);
}

/**
  * @brief  Legacy comment retained by the vendor application template.
  * @retval None
  */
/**
  * @brief  Enable DMA controller clock and acquire DMA resources.
  * @retval None
  */
static void MX_DMA_Init(void)
{
    /*
     * HPDMA3 clock enable.
     */
    if (ResMgr_Request(RESMGR_RESOURCE_RIF_RCC,
                       RESMGR_RCC_RESOURCE(85))
        == RESMGR_STATUS_ACCESS_OK)
    {
        __HAL_RCC_HPDMA3_CLK_ENABLE();
    }

    /*
     * Acquire HPDMA3 Channel 2.
     */
    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIF_HPDMA3,
                       RESMGR_HPDMA_CHANNEL(2)))
    {
        Error_Handler();
    }

    /*
     * Acquire HPDMA3 Channel 3.
     */
    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIF_HPDMA3,
                       RESMGR_HPDMA_CHANNEL(3)))
    {
        Error_Handler();
    }
}

/**
  * @brief  I2C4 Initialization Function.
  * @retval None
  */
static void MX_I2C4_Init(void)
{
    /*
     * Acquire I2C4.
     */
    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIFSC,
                       STM32MP25_RIFSC_I2C4_ID))
    {
        Error_Handler();
    }

    /*
     * PD10 = I2C4_SDA.
     */
    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIF_GPIOD,
                       RESMGR_GPIO_PIN(10)))
    {
        Error_Handler();
    }

    /*
     * PD11 = I2C4_SCL.
     */
    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIF_GPIOD,
                       RESMGR_GPIO_PIN(11)))
    {
        Error_Handler();
    }

    /*
     * GPIOD clock.
     */
    if (RESMGR_STATUS_ACCESS_OK ==
        ResMgr_Request(RESMGR_RESOURCE_RIF_RCC,
                       RESMGR_RCC_RESOURCE(93)))
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }

    /*
     * I2C4 bus clock. The enable (and force/release reset) happens inside
     * HAL_I2C_Init -> HAL_I2C_MspInit. The RCC clock resource (unified
     * resource id 44, same number as the RIFSC peripheral id) should be
     * owned first: an unowned enable can be gated again by the secure side,
     * and every later I2C4 register access then raises a SERC illegal access
     * that locks the core in BusFault_Handler. A denial is not fatal - the
     * grant is best-effort hardening on top of the RIFSC peripheral grant.
     */
    if (RESMGR_STATUS_ACCESS_OK ==
        ResMgr_Request(RESMGR_RESOURCE_RIF_RCC,
                       RESMGR_RCC_RESOURCE(STM32MP25_RIFSC_I2C4_ID)))
    {
        I2c4ClockResourceAcquired = 1U;
        SendReply("M33: I2C4 clock resource granted\r\n");
    }
    else
    {
        I2c4ClockResourceAcquired = 0U;
        SendReply("M33: I2C4 clock resource DENIED; continuing\r\n");
    }

    /*
     * I2C4 configuration.
     */
    I2cHandle.Instance = I2C4;

    I2cHandle.Init.Timing = BUS_I2Cx_FREQUENCY;
    I2cHandle.Init.OwnAddress1 = 0;
    I2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    I2cHandle.Init.OwnAddress2 = 0;
    I2cHandle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&I2cHandle) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Analog filter.
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&I2cHandle,
                                     I2C_ANALOGFILTER_ENABLE)
        != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Digital filter.
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&I2cHandle,
                                      0)
        != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Fast Mode Plus.
     */
    if (HAL_I2CEx_ConfigFastModePlus(&I2cHandle,
                                     I2C_FASTMODEPLUS_ENABLE)
        != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Configure PD10 / PD11.
     */
    HAL_I2C_MspPostInit(&I2cHandle);
}

/**
  * @brief  I2C4 De-Initialization Function.
  * @retval None
  */
static void MX_I2C4_DeInit(void)
{
    HAL_I2C_DeInit(&I2cHandle);

    if (I2c4ClockResourceAcquired != 0U)
    {
        (void)ResMgr_Release(RESMGR_RESOURCE_RIF_RCC,
                             RESMGR_RCC_RESOURCE(STM32MP25_RIFSC_I2C4_ID));
        I2c4ClockResourceAcquired = 0U;
    }

    (void)ResMgr_Release(RESMGR_RESOURCE_RIF_HPDMA3,
                         RESMGR_HPDMA_CHANNEL(2));

    (void)ResMgr_Release(RESMGR_RESOURCE_RIF_HPDMA3,
                         RESMGR_HPDMA_CHANNEL(3));

    (void)ResMgr_Release(RESMGR_RESOURCE_RIFSC,
                         STM32MP25_RIFSC_I2C4_ID);

    (void)ResMgr_Release(RESMGR_RESOURCE_RIF_GPIOD,
                         RESMGR_GPIO_PIN(10));

    (void)ResMgr_Release(RESMGR_RESOURCE_RIF_GPIOD,
                         RESMGR_GPIO_PIN(11));
}

/**
  * @brief  IPCC Initialization Function.
  * @retval None
  */
static void MX_IPCC_Init(void)
{
    hipcc.Instance = IPCC1;

    if (HAL_IPCC_Init(&hipcc) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(IPCC1_RX_IRQn,
                         DEFAULT_IRQ_PRIO,
                         0);

    HAL_NVIC_EnableIRQ(IPCC1_RX_IRQn);
}

/**
  * @brief  Rx complete callback for Virtual UART.
  *
  * The shared CommandService parses the optional SEQ and dispatches payloads.
  *
  * @param  huart Virtual UART handle.
  * @retval None
  */
void VIRT_UART0_RxCpltCallback(VIRT_UART_HandleTypeDef *huart)
{
    char command[COMMAND_MAX_LEN + 1U];
    uint16_t len;
    uint8_t next;

    /*
     * Copy received RPMsg data into a local, null-terminated buffer.
     */
    len = huart->RxXferSize;

    if (len >= sizeof(command))
    {
        len = sizeof(command) - 1U;
    }

    memcpy(command,
           huart->pRxBuffPtr,
           len);

    command[len] = '\0';

    /*
     * Remove CR/LF.
     */
    command[strcspn(command, "\r\n")] = '\0';

    /*
     * Queue only; dispatch runs in the main loop so command handlers may
     * block on actuator I2C without freezing the HAL tick or RPMsg. A full
     * queue drops the frame - the A35 client deadline reports the loss.
     */
    next = (uint8_t)((CommandQueueHead + 1U) & (COMMAND_QUEUE_DEPTH - 1U));
    if (next == CommandQueueTail)
    {
        return;
    }

    memcpy(CommandQueue[CommandQueueHead],
           command,
           strlen(command) + 1U);
    CommandQueueHead = next;
}

/**
  * @brief  I2C8 Initialization Function.
  * @retval None
  */
static uint8_t MX_I2C8_Init(void)
{
    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIFSC,
                       STM32MP25_RIFSC_I2C8_ID))
    {
        loc_printf("I2C8 unavailable: RIFSC resource\r\n");
        return 0U;
    }

    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIF_GPIOZ,
                       RESMGR_GPIO_PIN(I2C8_SDA_PIN)))
    {
        loc_printf("I2C8 unavailable: SDA GPIO resource\r\n");
        return 0U;
    }

    if (RESMGR_STATUS_ACCESS_OK !=
        ResMgr_Request(RESMGR_RESOURCE_RIF_GPIOZ,
                       RESMGR_GPIO_PIN(I2C8_SCL_PIN)))
    {
        loc_printf("I2C8 unavailable: SCL GPIO resource\r\n");
        return 0U;
    }

    if (RESMGR_STATUS_ACCESS_OK ==
        ResMgr_Request(RESMGR_RESOURCE_RIF_RCC,
                       RESMGR_RCC_RESOURCE(I2C8_RCC_RESOURCE)))
    {
        I2c8RccResourceAcquired = 1U;
        __HAL_RCC_GPIOZ_CLK_ENABLE();
        loc_printf("I2C8: RCC resource 101 acquired\r\n");
    }
    else
    {
        I2c8RccResourceAcquired = 0U;
        loc_printf("I2C8: RCC resource 101 unavailable; continuing\r\n");
    }

    /*
     * I2C8 bus clock (unified resource id 48, same number as the RIFSC
     * peripheral id); enabled inside HAL_I2C_Init -> HAL_I2C_MspInit.
     * Same unowned-clock exposure as I2C4: acquire it explicitly.
     */
    if (RESMGR_STATUS_ACCESS_OK ==
        ResMgr_Request(RESMGR_RESOURCE_RIF_RCC,
                       RESMGR_RCC_RESOURCE(STM32MP25_RIFSC_I2C8_ID)))
    {
        I2c8BusClockResourceAcquired = 1U;
    }
    else
    {
        I2c8BusClockResourceAcquired = 0U;
        SendReply("M33: I2C8 bus-clock resource DENIED; continuing\r\n");
    }

    I2c8Handle.Instance = I2C8;
    I2c8Handle.Init.Timing = BUS_I2Cx_FREQUENCY;
    I2c8Handle.Init.OwnAddress1 = 0;
    I2c8Handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    I2c8Handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    I2c8Handle.Init.OwnAddress2 = 0;
    I2c8Handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    I2c8Handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    I2c8Handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&I2c8Handle) != HAL_OK)
    {
        loc_printf("I2C8 unavailable: HAL init\r\n");
        return 0U;
    }
    I2c8HalInitialized = 1U;

    if (HAL_I2CEx_ConfigAnalogFilter(&I2c8Handle,
                                     I2C_ANALOGFILTER_ENABLE)
        != HAL_OK)
    {
        loc_printf("I2C8 unavailable: analog filter\r\n");
        return 0U;
    }

    if (HAL_I2CEx_ConfigDigitalFilter(&I2c8Handle, 0) != HAL_OK)
    {
        loc_printf("I2C8 unavailable: digital filter\r\n");
        return 0U;
    }

    HAL_I2C_MspPostInit(&I2c8Handle);
    return 1U;
}

/**
  * @brief  I2C8 De-Initialization Function.
  * @retval None
  */
static void MX_I2C8_DeInit(void)
{
    if (I2c8HalInitialized != 0U)
    {
        HAL_I2C_DeInit(&I2c8Handle);
        I2c8HalInitialized = 0U;
    }

    (void)ResMgr_Release(RESMGR_RESOURCE_RIFSC,
                         STM32MP25_RIFSC_I2C8_ID);

    (void)ResMgr_Release(RESMGR_RESOURCE_RIF_GPIOZ,
                         RESMGR_GPIO_PIN(I2C8_SDA_PIN));

    (void)ResMgr_Release(RESMGR_RESOURCE_RIF_GPIOZ,
                         RESMGR_GPIO_PIN(I2C8_SCL_PIN));

    if (I2c8RccResourceAcquired != 0U)
    {
        (void)ResMgr_Release(RESMGR_RESOURCE_RIF_RCC,
                             RESMGR_RCC_RESOURCE(I2C8_RCC_RESOURCE));
        I2c8RccResourceAcquired = 0U;
    }

    if (I2c8BusClockResourceAcquired != 0U)
    {
        (void)ResMgr_Release(RESMGR_RESOURCE_RIF_RCC,
                             RESMGR_RCC_RESOURCE(STM32MP25_RIFSC_I2C8_ID));
        I2c8BusClockResourceAcquired = 0U;
    }
}

/**
  * @brief  I2C Tx Transfer completed callback.
  * @retval None
  */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
    (void)I2cHandle;
}

/**
  * @brief  I2C Rx Transfer completed callback.
  * @retval None
  */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
    (void)I2cHandle;
}

/**
  * @brief  I2C error callback.
  * @retval None
  */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *I2cHandle)
{
    /*
     * Preserve behavior of the verified I2C4/AT24C64 project.
     */
    if (HAL_I2C_GetError(I2cHandle) != HAL_I2C_ERROR_AF)
    {
        Error_Handler();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    DYP_UART_RxCpltCallback(uart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    DYP_UART_ErrorCallback(uart);
}

/**
  * @brief  Callback from IPCC Interrupt Handler:
  *         Remote Processor asks local processor to shutdown.
  * @retval None
  */
void CoproSync_ShutdownCb(IPCC_HandleTypeDef *hipcc,
                          uint32_t ChannelIndex,
                          IPCC_CHANNELDirTypeDef ChannelDir)
{
    (void)ChannelDir;

    VIRT_UART_DeInit(&huart0);

    SensorService_DeInitDyp();

    OPENAMP_DeInit();

    MX_I2C8_DeInit();

    MX_I2C4_DeInit();

    HAL_IPCC_NotifyCPU(hipcc,
                       ChannelIndex,
                       IPCC_CHANNEL_DIR_RX);

    while (1)
    {
    }
}

/**
  * @brief  Error Handler.
  * @retval None
  */
void Error_Handler(void)
{
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and line number.
  * @param  file
  * @param  line
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;

    while (1)
    {
    }
}

#endif /* USE_FULL_ASSERT */
