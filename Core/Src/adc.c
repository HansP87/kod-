/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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
#include "adc.h"

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  ADC_MultiModeTypeDef multi_mode = {0};
  ADC_ChannelConfTypeDef channel_config = {0};

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_16B;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 4;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_LPTIM1_OUT;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multi_mode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multi_mode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_0;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  channel_config.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_1;
  channel_config.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_4;
  channel_config.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_18;
  channel_config.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
      Error_Handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  {
      Error_Handler();
  }

}
/* ADC2 init function */
void MX_ADC2_Init(void)
{

  ADC_ChannelConfTypeDef channel_config = {0};

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_16B;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 4;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_EXTERNALTRIG_LPTIM1_OUT;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_0;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  channel_config.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc2, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_1;
  channel_config.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc2, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_TEMPSENSOR;
  channel_config.Rank = ADC_REGULAR_RANK_3;
  channel_config.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  channel_config.Channel = ADC_CHANNEL_VREFINT;
  channel_config.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc2, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
      Error_Handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  {
      Error_Handler();
  }

}

static uint32_t hal_rcc_adc12_clk_enabled=0;

void HAL_ADC_MspInit(ADC_HandleTypeDef* adc_handle)
{

  GPIO_InitTypeDef gpio_init_struct = {0};
  RCC_PeriphCLKInitTypeDef periph_clk_init_struct = {0};
  if(adc_handle->Instance==ADC1)
  {

  /** Initializes the peripherals clock
  */
    periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph_clk_init_struct.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC1 clock enable */
    hal_rcc_adc12_clk_enabled++;
    if(hal_rcc_adc12_clk_enabled==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA4     ------> ADC1_INP18
    PC4     ------> ADC1_INP4
    PA0_C     ------> ADC1_INP0
    PA1_C     ------> ADC1_INP1
    */
    gpio_init_struct.Pin = GPIO_PIN_4;
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;
    gpio_init_struct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_4;
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;
    gpio_init_struct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpio_init_struct);

    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA0, SYSCFG_SWITCH_PA0_OPEN);

    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA1, SYSCFG_SWITCH_PA1_OPEN);

    /* ADC1 DMA Init */
    /* ADC1 Init */
    hdma_adc1.Instance = DMA1_Stream0;
    hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adc_handle,DMA_Handle,hdma_adc1);

  }
  else if(adc_handle->Instance==ADC2)
  {

  /** Initializes the peripherals clock
  */
    periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph_clk_init_struct.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC2 clock enable */
    hal_rcc_adc12_clk_enabled++;
    if(hal_rcc_adc12_clk_enabled==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**ADC2 GPIO Configuration
    PC2_C     ------> ADC2_INP0
    PC3_C     ------> ADC2_INP1
    */
    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_OPEN);

    HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC3, SYSCFG_SWITCH_PC3_OPEN);

    /* ADC2 DMA Init */
    /* ADC2 Init */
    hdma_adc2.Instance = DMA1_Stream3;
    hdma_adc2.Init.Request = DMA_REQUEST_ADC2;
    hdma_adc2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc2.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc2.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc2.Init.Mode = DMA_CIRCULAR;
    hdma_adc2.Init.Priority = DMA_PRIORITY_LOW;
    hdma_adc2.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc2) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adc_handle,DMA_Handle,hdma_adc2);

  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adc_handle)
{

  if(adc_handle->Instance==ADC1)
  {

    /* Peripheral clock disable */
    hal_rcc_adc12_clk_enabled--;
    if(hal_rcc_adc12_clk_enabled==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC1 GPIO Configuration
    PA4     ------> ADC1_INP18
    PC4     ------> ADC1_INP4
    PA0_C     ------> ADC1_INP0
    PA1_C     ------> ADC1_INP1
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_4);

    /* ADC1 DMA DeInit */
    HAL_DMA_DeInit(adc_handle->DMA_Handle);

  }
  else if(adc_handle->Instance==ADC2)
  {

    /* Peripheral clock disable */
    hal_rcc_adc12_clk_enabled--;
    if(hal_rcc_adc12_clk_enabled==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /* ADC2 DMA DeInit */
    HAL_DMA_DeInit(adc_handle->DMA_Handle);

  }
}

