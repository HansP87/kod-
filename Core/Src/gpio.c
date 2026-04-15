/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/

/** Configure pins */
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef gpio_init_struct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, WIFI_BOOT_Pin|WIFI_WKUP_Pin|WIFI_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, USER_LED1_Pin|USER_LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(AUDIO_NRST_GPIO_Port, AUDIO_NRST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : WIFI_GPIO_Pin WIFI_DATRDY_Pin */
  gpio_init_struct.Pin = WIFI_GPIO_Pin|WIFI_DATRDY_Pin;
  gpio_init_struct.Mode = GPIO_MODE_IT_RISING;
  gpio_init_struct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOI, &gpio_init_struct);

  /*Configure GPIO pins : SDNCAS_Pin SDCLK_Pin A15_Pin A14_Pin
                           A11_Pin A10_Pin */
  gpio_init_struct.Pin = SDNCAS_Pin|SDCLK_Pin|A15_Pin|A14_Pin
                          |A11_Pin|A10_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_FMC;
  HAL_GPIO_Init(GPIOG, &gpio_init_struct);

  /*Configure GPIO pins : I2S6_SDO_Pin I2S6_SDI_Pin I2S6_CK_Pin */
  gpio_init_struct.Pin = I2S6_SDO_Pin|I2S6_SDI_Pin|I2S6_CK_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_struct.Alternate = GPIO_AF5_SPI6;
  HAL_GPIO_Init(GPIOG, &gpio_init_struct);

  /*Configure GPIO pin : OCSPI1_IO6_Pin */
  gpio_init_struct.Pin = OCSPI1_IO6_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF9_OCTOSPIM_P1;
  HAL_GPIO_Init(OCSPI1_IO6_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pin : OCSPI1_IO7_Pin */
  gpio_init_struct.Pin = OCSPI1_IO7_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF10_OCTOSPIM_P1;
  HAL_GPIO_Init(OCSPI1_IO7_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pins : D3_Pin D2_Pin D0_Pin D1_Pin
                           D13_Pin D15_Pin D14_Pin */
  gpio_init_struct.Pin = D3_Pin|D2_Pin|D0_Pin|D1_Pin
                          |D13_Pin|D15_Pin|D14_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_FMC;
  HAL_GPIO_Init(GPIOD, &gpio_init_struct);

  /*Configure GPIO pins : SDIO1_D2_Pin SDIO1_CK_Pin SDIO1_D3_Pin SDIO1_D1_Pin
                           SDIO1_D0_Pin */  gpio_init_struct.Pin = SDIO1_D2_Pin|SDIO1_CK_Pin|SDIO1_D3_Pin|SDIO1_D1_Pin
                          |SDIO1_D0_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(GPIOC, &gpio_init_struct);

  /*Configure GPIO pins : WIFI_BOOT_Pin WIFI_WKUP_Pin WIFI_RST_Pin */
  gpio_init_struct.Pin = WIFI_BOOT_Pin|WIFI_WKUP_Pin|WIFI_RST_Pin;
  gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &gpio_init_struct);

  /*Configure GPIO pins : FMC_NBL0_Pin FMC_NBL1_Pin D9_Pin D4_Pin
                           D10_Pin D11_Pin D7_Pin D6_Pin
                           D12_Pin D5_Pin D8_Pin */
  gpio_init_struct.Pin = FMC_NBL0_Pin|FMC_NBL1_Pin|D9_Pin|D4_Pin
                          |D10_Pin|D11_Pin|D7_Pin|D6_Pin
                          |D12_Pin|D5_Pin|D8_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_FMC;
  HAL_GPIO_Init(GPIOE, &gpio_init_struct);

  /*Configure GPIO pins : USER_LED1_Pin AUDIO_NRST_Pin USER_LED2_Pin */
  gpio_init_struct.Pin = USER_LED1_Pin|AUDIO_NRST_Pin|USER_LED2_Pin;
  gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &gpio_init_struct);

  /*Configure GPIO pin : SDIO1_CMD_Pin */
  gpio_init_struct.Pin = SDIO1_CMD_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(SDIO1_CMD_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pin : uSD_Detect_Pin */
  gpio_init_struct.Pin = uSD_Detect_Pin;
  gpio_init_struct.Mode = GPIO_MODE_IT_RISING;
  gpio_init_struct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(uSD_Detect_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pin : SPI2_SCK_Pin */
  gpio_init_struct.Pin = SPI2_SCK_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_struct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(SPI2_SCK_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pin : SPI2_NSS_Pin */
  gpio_init_struct.Pin = SPI2_NSS_Pin;
  gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio_init_struct);

  /*Configure GPIO pin : WAKEUP_Pin */
  gpio_init_struct.Pin = WAKEUP_Pin;
  gpio_init_struct.Mode = GPIO_MODE_IT_FALLING;
  gpio_init_struct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(WAKEUP_GPIO_Port, &gpio_init_struct);

  /* Enable EXTI interrupt for the wakeup button on PC13 */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /*Configure GPIO pins : A1_Pin A0_Pin A2_Pin A4_Pin
                           A3_Pin A5_Pin A7_Pin SDNRAS_Pin
                           A9_Pin A8_Pin A6_Pin */
  gpio_init_struct.Pin = A1_Pin|A0_Pin|A2_Pin|A4_Pin
                          |A3_Pin|A5_Pin|A7_Pin|SDNRAS_Pin
                          |A9_Pin|A8_Pin|A6_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_FMC;
  HAL_GPIO_Init(GPIOF, &gpio_init_struct);

  /*Configure GPIO pin : MCO_Pin */
  gpio_init_struct.Pin = MCO_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_struct.Alternate = GPIO_AF0_MCO;
  HAL_GPIO_Init(MCO_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pin : OCSPI1_NCS_Pin */
  gpio_init_struct.Pin = OCSPI1_NCS_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF10_OCTOSPIM_P1;
  HAL_GPIO_Init(OCSPI1_NCS_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pins : OCSPI1_IO3_Pin OCSPI1_IO2_Pin OCSPI1_IO1_Pin */
  gpio_init_struct.Pin = OCSPI1_IO3_Pin|OCSPI1_IO2_Pin|OCSPI1_IO1_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF10_OCTOSPIM_P1;
  HAL_GPIO_Init(GPIOF, &gpio_init_struct);

  /*Configure GPIO pins : SPI2_MISO_Pin SPI2_MOSI_Pin */
  gpio_init_struct.Pin = SPI2_MISO_Pin|SPI2_MOSI_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_struct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOC, &gpio_init_struct);

  /*Configure GPIO pins : I2C4_SDA_Pin I2C4_SCL_Pin */
  gpio_init_struct.Pin = I2C4_SDA_Pin|I2C4_SCL_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_OD;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_struct.Alternate = GPIO_AF4_I2C4;
  HAL_GPIO_Init(GPIOD, &gpio_init_struct);

  /*Configure GPIO pin : OCSPI1_IO0_Pin */
  gpio_init_struct.Pin = OCSPI1_IO0_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF9_OCTOSPIM_P1;
  HAL_GPIO_Init(OCSPI1_IO0_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pins : OCSPI1_IO4_Pin OCSPI1_DQS_Pin */
  gpio_init_struct.Pin = OCSPI1_IO4_Pin|OCSPI1_DQS_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF10_OCTOSPIM_P1;
  HAL_GPIO_Init(GPIOC, &gpio_init_struct);

  /*Configure GPIO pins : SDNE1_Pin SDNWE_Pin SDCKE1_Pin */
  gpio_init_struct.Pin = SDNE1_Pin|SDNWE_Pin|SDCKE1_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF12_FMC;
  HAL_GPIO_Init(GPIOH, &gpio_init_struct);

  /*Configure GPIO pin : OCSPI1_IO5_Pin */
  gpio_init_struct.Pin = OCSPI1_IO5_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF9_OCTOSPIM_P1;
  HAL_GPIO_Init(OCSPI1_IO5_GPIO_Port, &gpio_init_struct);

  /*Configure GPIO pins : I2S6_WS_Pin I2S6_MCK_Pin */
  gpio_init_struct.Pin = I2S6_WS_Pin|I2S6_MCK_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_struct.Alternate = GPIO_AF5_SPI6;
  HAL_GPIO_Init(GPIOA, &gpio_init_struct);

  /*Configure GPIO pin : OCSPI1_CLK_Pin */
  gpio_init_struct.Pin = OCSPI1_CLK_Pin;
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_struct.Alternate = GPIO_AF9_OCTOSPIM_P1;
  HAL_GPIO_Init(OCSPI1_CLK_GPIO_Port, &gpio_init_struct);

  /*AnalogSwitch Config */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA1, SYSCFG_SWITCH_PA1_CLOSE);

}

