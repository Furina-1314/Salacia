/**
  ******************************************************************************
  * @file    stm32mp2xx_it.c
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dyp.h"
#include "stm32mp2xx_it.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
/* External variables --------------------------------------------------------*/
extern IPCC_HandleTypeDef hipcc;
extern I2C_HandleTypeDef I2cHandle;

/******************************************************************************/
/*            Cortex-M33 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Secure Fault exception.
  * @param  None
  * @retval None
  */
void SecureFault_Handler(void)
{
  /* Go to infinite loop when Secure Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
* @brief  This function handles Debug Monitor exception.
* @param  None
* @retval None
*/
void DebugMon_Handler(void)
{
}

/**
* @brief  This function handles PendSVC exception.
* @param  None
* @retval None
*/
void PendSV_Handler(void)
{
}

/**
* @brief  This function handles SysTick Handler.
* @param  None
* @retval None
*/
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

void IPCC1_RX_IRQHandler(void)
{
   HAL_IPCC_RX_IRQHandler(&hipcc);
}

/******************************************************************************/
/*            Cortex-M33/A35 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
* @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/******************************************************************************/
/*                 STM32MP2xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/******************************************************************************/


/**
* @brief This function handles HPDMA3 channel2 global interrupt.
*/
void HPDMA3_Channel2_IRQHandler(void)
{
  /* USER CODE BEGIN HPDMA3_Channel2_IRQn 0 */

  /* USER CODE END HPDMA3_Channel2_IRQn 0 */
  HAL_DMA_IRQHandler(I2cHandle.hdmatx);
  /* USER CODE BEGIN HPDMA3_Channel2_IRQn 1 */

  /* USER CODE END HPDMA3_Channel2_IRQn 1 */
}

/**
* @brief This function handles HPDMA3 channel3 global interrupt.
*/
void HPDMA3_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN HPDMA3_Channel3_IRQn 0 */

  /* USER CODE END HPDMA3_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(I2cHandle.hdmarx);
  /* USER CODE BEGIN HPDMA3_Channel3_IRQn 1 */

  /* USER CODE END HPDMA3_Channel3_IRQn 1 */
}

/**
* @brief This function handles I2C8 interrupt.
*/
void I2C4_IRQHandler(void)
{
  /* USER CODE BEGIN I2C4_EV_IRQn 0 */

  /* USER CODE END I2C4_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&I2cHandle);
  HAL_I2C_ER_IRQHandler(&I2cHandle);
  /* USER CODE BEGIN I2C4_EV_IRQn 1 */

  /* USER CODE END I2C4_EV_IRQn 1 */
}

/**
* @brief This function handles UART4 global interrupt for DYP RX.
*/
void UART4_IRQHandler(void)
{
  DYP_UART_IRQHandler();
}
