/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
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
#include "tim.h"

TIM_HandleTypeDef htim2;

/* TIM2 init function */
void MX_TIM2_Init(void)
{

  TIM_SlaveConfigTypeDef slave_config = {0};
  TIM_MasterConfigTypeDef master_config = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 279;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  slave_config.SlaveMode = TIM_SLAVEMODE_DISABLE;
  slave_config.InputTrigger = TIM_TS_ITR0;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &slave_config) != HAL_OK)
  {
    Error_Handler();
  }
  master_config.MasterOutputTrigger = TIM_TRGO_RESET;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &master_config) != HAL_OK)
  {
    Error_Handler();
  }

}

/**
  * @brief Read the current TIM2 counter value in microseconds.
  * @return Free-running TIM2 timestamp in microseconds.
  */
uint32_t tim2_get_timestamp_us(void)
{
  return __HAL_TIM_GET_COUNTER(&htim2);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_base_handle)
{

  if(tim_base_handle->Instance==TIM2)
  {

    /* TIM2 clock enable */
    __HAL_RCC_TIM2_CLK_ENABLE();

  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_base_handle)
{

  if(tim_base_handle->Instance==TIM2)
  {

    /* Peripheral clock disable */
    __HAL_RCC_TIM2_CLK_DISABLE();

  }
}

