/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "cmsis_os2.h"
#include "usart.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

static osMutexId_t usart1_tx_mutex = NULL;
static const osMutexAttr_t usart1_tx_mutex_attributes = {
  .name = "usart1_tx_mutex"
};

static osMutexId_t usart1_get_tx_mutex(void);

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 250000;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

}

void HAL_UART_MspInit(UART_HandleTypeDef* uart_handle)
{

  GPIO_InitTypeDef gpio_init_struct = {0};
  RCC_PeriphCLKInitTypeDef periph_clk_init_struct = {0};
  if(uart_handle->Instance==USART1)
  {

  /** Initializes the peripherals clock
  */
    periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    periph_clk_init_struct.Usart16ClockSelection = RCC_USART16910CLKSOURCE_D2PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX
    */
    gpio_init_struct.Pin = VCP_RX_Pin|VCP_TX_Pin;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_struct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    hdma_usart1_rx.Instance = DMA1_Stream1;
    hdma_usart1_rx.Init.Request = DMA_REQUEST_USART1_RX;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uart_handle,hdmarx,hdma_usart1_rx);

    /* USART1_TX Init */
    hdma_usart1_tx.Instance = DMA1_Stream2;
    hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uart_handle,hdmatx,hdma_usart1_tx);

    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uart_handle)
{

  if(uart_handle->Instance==USART1)
  {

    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX
    */
    HAL_GPIO_DeInit(GPIOA, VCP_RX_Pin|VCP_TX_Pin);

    /* USART1 DMA DeInit */
    HAL_DMA_DeInit(uart_handle->hdmarx);
    HAL_DMA_DeInit(uart_handle->hdmatx);

    HAL_NVIC_DisableIRQ(USART1_IRQn);

  }
}

HAL_StatusTypeDef usart1_transmit_blocking(const uint8_t *data, uint16_t size, uint32_t timeout)
{
  HAL_StatusTypeDef status;
  osMutexId_t tx_mutex;

  if ((data == NULL) || (size == 0U))
  {
    return HAL_OK;
  }

  if (osKernelGetState() != osKernelRunning)
  {
    return HAL_UART_Transmit(&huart1, (uint8_t *)data, size, timeout);
  }

  tx_mutex = usart1_get_tx_mutex();
  if (tx_mutex == NULL)
  {
    return HAL_ERROR;
  }

  if (osMutexAcquire(tx_mutex, osWaitForever) != osOK)
  {
    return HAL_ERROR;
  }

  status = HAL_UART_Transmit(&huart1, (uint8_t *)data, size, timeout);

  if (osMutexRelease(tx_mutex) != osOK)
  {
    return HAL_ERROR;
  }

  return status;
}

static osMutexId_t usart1_get_tx_mutex(void)
{
  osMutexId_t current_mutex = __atomic_load_n(&usart1_tx_mutex, __ATOMIC_ACQUIRE);

  if (current_mutex == NULL)
  {
    osMutexId_t created_mutex = osMutexNew(&usart1_tx_mutex_attributes);

    if (created_mutex == NULL)
    {
      return NULL;
    }

    if (__atomic_compare_exchange_n(
            &usart1_tx_mutex,
            &current_mutex,
            created_mutex,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE) == 0)
    {
      (void)osMutexDelete(created_mutex);
    }
    else
    {
      current_mutex = created_mutex;
    }
  }

  return current_mutex;
}

