/**
 ******************************************************************************
 * @file    lptim.c
 * @brief   This file provides code for the configuration
 *          of the LPTIM instances.
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
#include "lptim.h"

LPTIM_HandleTypeDef hlptim1;
LPTIM_HandleTypeDef hlptim2;

/* LPTIM1 init function */
void MX_LPTIM1_Init(void)
{

	hlptim1.Instance = LPTIM1;
	hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
	hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
	hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
	hlptim1.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
	hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
	hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
	hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
	hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
	if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
	{
		Error_Handler();
	}

}
/* LPTIM2 init function */
void MX_LPTIM2_Init(void)
{

	hlptim2.Instance = LPTIM2;
	hlptim2.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
	hlptim2.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
	hlptim2.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
	hlptim2.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
	hlptim2.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
	hlptim2.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
	hlptim2.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
	hlptim2.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
	if (HAL_LPTIM_Init(&hlptim2) != HAL_OK)
	{
		Error_Handler();
	}

}

void HAL_LPTIM_MspInit(LPTIM_HandleTypeDef* lptim_handle)
{

	RCC_PeriphCLKInitTypeDef periph_clk_init_struct = {0};
	if(lptim_handle->Instance==LPTIM1)
	{

		/** Initializes the peripherals clock
		 */
		periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
		periph_clk_init_struct.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_CLKP;
		if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) != HAL_OK)
		{
			Error_Handler();
		}

		/* LPTIM1 clock enable */
		__HAL_RCC_LPTIM1_CLK_ENABLE();

	}
	else if(lptim_handle->Instance==LPTIM2)
	{

		/** Initializes the peripherals clock
		 */
		periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM2;
		periph_clk_init_struct.Lptim2ClockSelection = RCC_LPTIM2CLKSOURCE_CLKP;
		if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) != HAL_OK)
		{
			Error_Handler();
		}

		/* LPTIM2 clock enable */
		__HAL_RCC_LPTIM2_CLK_ENABLE();
		HAL_NVIC_SetPriority(LPTIM2_IRQn, 5, 0);
		HAL_NVIC_EnableIRQ(LPTIM2_IRQn);
	}
}

void HAL_LPTIM_MspDeInit(LPTIM_HandleTypeDef* lptim_handle)
{

	if(lptim_handle->Instance==LPTIM1)
	{

		/* Peripheral clock disable */
		__HAL_RCC_LPTIM1_CLK_DISABLE();

	}
	else if(lptim_handle->Instance==LPTIM2)
	{

		/* Peripheral clock disable */
		__HAL_RCC_LPTIM2_CLK_DISABLE();

	}
}

