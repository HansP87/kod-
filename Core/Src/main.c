/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "adc.h"
#include "crc.h"
#include "dma.h"
#include "lptim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
void MX_FREERTOS_Init(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_CRC_Init();
  MX_LPTIM1_Init();
  MX_LPTIM2_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_ADC2_Init();
  HAL_TIM_Base_Start(&htim2);

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  while (1)
  {

  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef rcc_osc_init_struct = {0};
  RCC_ClkInitTypeDef rcc_clk_init_struct = {0};

  /*AXI clock gating */
  RCC->CKGAENR = 0xE003FFFF;

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  rcc_osc_init_struct.HSEState = RCC_HSE_ON;
  rcc_osc_init_struct.PLL.PLLState = RCC_PLL_ON;
  rcc_osc_init_struct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  rcc_osc_init_struct.PLL.PLLM = 3;
  rcc_osc_init_struct.PLL.PLLN = 70;
  rcc_osc_init_struct.PLL.PLLP = 2;
  rcc_osc_init_struct.PLL.PLLQ = 3;
  rcc_osc_init_struct.PLL.PLLR = 4;
  rcc_osc_init_struct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  rcc_osc_init_struct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  rcc_osc_init_struct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&rcc_osc_init_struct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  rcc_clk_init_struct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  rcc_clk_init_struct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  rcc_clk_init_struct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  rcc_clk_init_struct.AHBCLKDivider = RCC_HCLK_DIV1;
  rcc_clk_init_struct.APB3CLKDivider = RCC_APB3_DIV2;
  rcc_clk_init_struct.APB1CLKDivider = RCC_APB1_DIV2;
  rcc_clk_init_struct.APB2CLKDivider = RCC_APB2_DIV2;
  rcc_clk_init_struct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&rcc_clk_init_struct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Configure shared peripheral clocks used across the application.
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef periph_clk_init_struct = {0};

  periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_CKPER;
  periph_clk_init_struct.CkperClockSelection = RCC_CLKPSOURCE_HSE;

  if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */
