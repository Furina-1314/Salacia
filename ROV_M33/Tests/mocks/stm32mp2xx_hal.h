#ifndef TEST_STM32MP2XX_HAL_H
#define TEST_STM32MP2XX_HAL_H

#include <stdint.h>

#define HAL_I2C_ERROR_NONE       0U
#define I2C_MEMADD_SIZE_8BIT     1U

typedef struct
{
    uint32_t placeholder;
} I2C_HandleTypeDef;

typedef struct
{
    uint32_t placeholder;
} USART_TypeDef;

typedef struct
{
    uint32_t placeholder;
} GPIO_TypeDef;

typedef struct
{
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
    uint32_t HwFlowCtl;
    uint32_t OverSampling;
    uint32_t OneBitSampling;
    uint32_t ClockPrescaler;
} UART_InitTypeDef;

typedef struct
{
    uint32_t AdvFeatureInit;
} UART_AdvFeatureInitTypeDef;

typedef struct
{
    USART_TypeDef *Instance;
    UART_InitTypeDef Init;
    UART_AdvFeatureInitTypeDef AdvancedInit;
} UART_HandleTypeDef;

typedef struct
{
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
    uint32_t Alternate;
} GPIO_InitTypeDef;

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *i2c,
                                        uint16_t address,
                                        uint32_t trials,
                                        uint32_t timeout);

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *i2c,
                                    uint16_t address,
                                    uint16_t reg,
                                    uint16_t mem_address_size,
                                    uint8_t *data,
                                    uint16_t size,
                                    uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *i2c,
                                   uint16_t address,
                                   uint16_t reg,
                                   uint16_t mem_address_size,
                                   uint8_t *data,
                                   uint16_t size,
                                   uint32_t timeout);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *i2c);
void HAL_Delay(uint32_t delay_ms);

#define GPIO_PIN_6                    (1UL << 6)
#define GPIO_PIN_7                    (1UL << 7)
#define GPIO_MODE_OUTPUT_PP           1U
#define GPIO_MODE_AF_PP               2U
#define GPIO_PULLUP                   1U
#define GPIO_SPEED_FREQ_HIGH          3U
#define GPIO_AF3_UART4                3U
#define GPIO_PIN_RESET                0U
#define GPIO_PIN_SET                  1U

#define UART_WORDLENGTH_8B            0U
#define UART_STOPBITS_1               0U
#define UART_PARITY_NONE              0U
#define UART_MODE_RX                  1U
#define UART_HWCONTROL_NONE           0U
#define UART_OVERSAMPLING_8           1U
#define UART_ONE_BIT_SAMPLE_DISABLE   0U
#define UART_PRESCALER_DIV1           0U
#define UART_ADVFEATURE_NO_INIT       0U

#define UART4_IRQn                    126

extern USART_TypeDef test_uart4_instance;
extern GPIO_TypeDef test_gpiob_instance;

#define UART4                         (&test_uart4_instance)
#define GPIOB                         (&test_gpiob_instance)

uint8_t Test_HAL_RCC_GPIOB_IsEnabled(void);
void Test_HAL_RCC_GPIOB_Enable(void);
void Test_HAL_RCC_GPIOB_Disable(void);
void Test_HAL_RCC_UART4_Enable(void);
void Test_HAL_RCC_UART4_Disable(void);

#define __HAL_RCC_GPIOB_IS_CLK_ENABLED() Test_HAL_RCC_GPIOB_IsEnabled()
#define __HAL_RCC_GPIOB_CLK_ENABLE()     Test_HAL_RCC_GPIOB_Enable()
#define __HAL_RCC_GPIOB_CLK_DISABLE()    Test_HAL_RCC_GPIOB_Disable()
#define __HAL_RCC_UART4_CLK_ENABLE()     Test_HAL_RCC_UART4_Enable()
#define __HAL_RCC_UART4_CLK_DISABLE()    Test_HAL_RCC_UART4_Disable()

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, uint32_t state);
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *init);
void HAL_GPIO_DeInit(GPIO_TypeDef *port, uint32_t pin);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,
                                      uint8_t *data,
                                      uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive_IT(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart);
void HAL_UART_IRQHandler(UART_HandleTypeDef *uart);
uint32_t HAL_UART_GetError(UART_HandleTypeDef *uart);
uint32_t HAL_GetTick(void);
void HAL_NVIC_SetPriority(int32_t irq, uint32_t priority,
                          uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int32_t irq);
void HAL_NVIC_DisableIRQ(int32_t irq);

#endif
