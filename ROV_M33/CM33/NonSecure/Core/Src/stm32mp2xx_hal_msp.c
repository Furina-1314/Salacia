/**
  ******************************************************************************
  * @file    stm32mp2xx_hal_msp.c
  * @author  MCD Application Team
  * @brief   HAL MSP module.
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
#include "res_mgr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static DMA_HandleTypeDef hdma_tx;
static DMA_HandleTypeDef hdma_rx;

/* Private function prototypes -----------------------------------------------*/
extern void Error_Handler(void);

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

/**
* @brief IPCC MSP Initialization
* This function configures the hardware resources used in this example
* @param hipcc: IPCC handle pointer
  * @retval None
  */
void HAL_IPCC_MspInit(IPCC_HandleTypeDef* hipcc)
{

  if(hipcc->Instance==IPCC1)
  {
  /* USER CODE BEGIN IPCC_MspInit 0 */

  /* USER CODE END IPCC_MspInit 0 */
    /* Peripheral clock enable */
  if(ResMgr_Request(RESMGR_RESOURCE_RIF_RCC, RESMGR_RCC_RESOURCE(87)) == RESMGR_STATUS_ACCESS_OK)
  {
    __HAL_RCC_IPCC1_CLK_ENABLE();
  }
  else
  {
	/* RCC->IPCC1CFGR already enabled? */
	if(READ_BIT(RCC->IPCC1CFGR, RCC_IPCC1CFGR_IPCC1EN) != RCC_IPCC1CFGR_IPCC1EN)
	{
	  Error_Handler();
	}
  }

  /* USER CODE BEGIN IPCC_MspInit 1 */

  /* USER CODE END IPCC_MspInit 1 */
}

}

/**
* @brief IPCC MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hipcc: IPCC handle pointer
* @retval None
*/

void HAL_IPCC_MspDeInit(IPCC_HandleTypeDef* hipcc)
{

  if(hipcc->Instance==IPCC1)
  {
  /* USER CODE BEGIN IPCC_MspDeInit 0 */

  /* USER CODE END IPCC_MspDeInit 0 */
    /* Peripheral clock disable */
	if(ResMgr_Request(RESMGR_RESOURCE_RIF_RCC, RESMGR_RCC_RESOURCE(87)) == RESMGR_STATUS_ACCESS_OK)
	{
      __HAL_RCC_IPCC1_CLK_DISABLE();
      ResMgr_Release(RESMGR_RESOURCE_RIF_RCC, RESMGR_RCC_RESOURCE(87));
	}

    /* IPCC interrupt DeInit */
    HAL_NVIC_DisableIRQ(IPCC1_RX_IRQn);
  /* USER CODE BEGIN IPCC_MspDeInit 1 */

  /* USER CODE END IPCC_MspDeInit 1 */
  }

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
  if(I2C4 == hi2c->Instance)
  {
    /* USER CODE BEGIN I2C4_MspInit 0 */
    /* USER CODE END I2C4_MspInit 0 */

	__HAL_RCC_I2C4_CLK_ENABLE();
	__HAL_RCC_I2C4_FORCE_RESET();
	__HAL_RCC_I2C4_RELEASE_RESET();

    /* I2C4 DMA Init */
    /* I2C4_TX Init */
    hdma_tx.Instance = HPDMA3_Channel2;
    hdma_tx.Init.Request = HPDMA_REQUEST_I2C4_TX;
    hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tx.Init.SrcInc = DMA_SINC_INCREMENTED;
    hdma_tx.Init.DestInc = DMA_DINC_FIXED;
    hdma_tx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    hdma_tx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    hdma_tx.Init.SrcBurstLength = 1;
    hdma_tx.Init.DestBurstLength = 1;
    hdma_tx.Init.Priority = DMA_HIGH_PRIORITY;
    hdma_tx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    /* Deinitialize the Stream for new transfer */
    HAL_DMA_DeInit(&hdma_tx);

    /* Configure the DMA Stream */
    if (HAL_OK != HAL_DMA_Init(&hdma_tx))
    {
      Error_Handler();
    }

    __HAL_LINKDMA(hi2c, hdmatx, hdma_tx);

    /* I2C4_RX Init */
    hdma_rx.Instance = HPDMA3_Channel3;
    hdma_rx.Init.Request = HPDMA_REQUEST_I2C4_RX;
    hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_rx.Init.SrcInc = DMA_SINC_FIXED;
    hdma_rx.Init.DestInc = DMA_DINC_INCREMENTED;
    hdma_rx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    hdma_rx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    hdma_rx.Init.SrcBurstLength = 1;
    hdma_rx.Init.DestBurstLength = 1;
    hdma_rx.Init.Priority = DMA_HIGH_PRIORITY;
    hdma_rx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;

    HAL_DMA_DeInit(&hdma_rx);
    if (HAL_OK != HAL_DMA_Init(&hdma_rx))
    {
      Error_Handler();
    }

    __HAL_LINKDMA(hi2c, hdmarx, hdma_rx);

    /* I2C4 interrupt Init */
    HAL_NVIC_SetPriority(I2C4_IRQn, DEFAULT_IRQ_PRIO, 0);
    HAL_NVIC_EnableIRQ(I2C4_IRQn);

    /* NVIC configuration for DMA transfer complete interrupt */
    HAL_NVIC_SetPriority(HPDMA3_Channel2_IRQn, 0x00, 0);
    HAL_NVIC_EnableIRQ(HPDMA3_Channel2_IRQn);

    /* NVIC configuration for DMA transfer complete interrupt */
    HAL_NVIC_SetPriority(HPDMA3_Channel3_IRQn, 0x00, 0);
    HAL_NVIC_EnableIRQ(HPDMA3_Channel3_IRQn);
    /* USER CODE BEGIN I2C4_MspInit 1 */
    /* USER CODE END I2C4_MspInit 1 */
  }
  else if(I2C8 == hi2c->Instance)
  {
    __HAL_RCC_I2C8_CLK_ENABLE();
    __HAL_RCC_I2C8_FORCE_RESET();
    __HAL_RCC_I2C8_RELEASE_RESET();

    /* I2C8 uses blocking transfers. No DMA or IRQ is configured. */
  }

}

void HAL_I2C_MspPostInit(I2C_HandleTypeDef* hi2c)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	/* USER CODE BEGIN I2C_MspPostInit 0 */

	/* USER CODE END I2C_MspPostInit 0 */
	if(I2C4 == hi2c->Instance)
	{
	    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
	    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	    GPIO_InitStruct.Pull = GPIO_NOPULL;
	    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	    GPIO_InitStruct.Alternate = GPIO_AF6_I2C4;
	    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	}
	else if(I2C8 == hi2c->Instance)
	{
	    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_9;
	    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	    GPIO_InitStruct.Pull = GPIO_NOPULL;
	    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	    GPIO_InitStruct.Alternate = GPIO_AF8_I2C8;
	    HAL_GPIO_Init(GPIOZ, &GPIO_InitStruct);
	}
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
  /* USER CODE BEGIN I2C4_MspDeInit 0 */

  /* USER CODE END I2C4_MspDeInit 0 */
  if(I2C4 == hi2c->Instance)
  {
	/* Peripheral clock disable */
  __HAL_RCC_I2C4_CLK_DISABLE();

    /**I2C4 GPIO Configuration
    PD10      ------> I2C4_SDA
    PD11      ------> I2C4_SCL
    */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_10 | GPIO_PIN_11);

    /* I2C4 DMA DeInit */
    HAL_DMA_DeInit(hi2c->hdmatx);
    HAL_DMA_DeInit(hi2c->hdmarx);

    /* I2C4 interrupt DeInit */
    HAL_NVIC_DisableIRQ(I2C4_IRQn);

    /* DMA interrupt DeInit */
    HAL_NVIC_DisableIRQ(HPDMA3_Channel2_IRQn);
    HAL_NVIC_DisableIRQ(HPDMA3_Channel3_IRQn);

    /* USER CODE BEGIN I2C4_MspDeInit 1 */
    /* USER CODE END I2C4_MspDeInit 1 */
  }
  else if(I2C8 == hi2c->Instance)
  {
    __HAL_RCC_I2C8_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOZ, GPIO_PIN_4 | GPIO_PIN_9);
  }

}

