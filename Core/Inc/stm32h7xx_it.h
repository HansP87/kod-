/**
  ******************************************************************************
  * @file    stm32h7xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32H7xx_IT_H
#define __STM32H7xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief Handle the non-maskable interrupt.
  */
void NMI_Handler(void);

/**
  * @brief Handle the hard fault exception.
  */
void HardFault_Handler(void);

/**
  * @brief Handle the memory management fault exception.
  */
void MemManage_Handler(void);

/**
  * @brief Handle the bus fault exception.
  */
void BusFault_Handler(void);

/**
  * @brief Handle the usage fault exception.
  */
void UsageFault_Handler(void);

/**
  * @brief Handle the debug monitor exception.
  */
void DebugMon_Handler(void);

/**
  * @brief Handle DMA1 stream 0 interrupts for ADC1.
  */
void DMA1_Stream0_IRQHandler(void);

/**
  * @brief Handle DMA1 stream 1 interrupts for USART1 RX.
  */
void DMA1_Stream1_IRQHandler(void);

/**
  * @brief Handle DMA1 stream 2 interrupts for USART1 TX.
  */
void DMA1_Stream2_IRQHandler(void);

/**
  * @brief Handle DMA1 stream 3 interrupts for ADC2.
  */
void DMA1_Stream3_IRQHandler(void);

/**
  * @brief Handle TIM1 update interrupts.
  */
void TIM1_UP_IRQHandler(void);

/**
  * @brief Handle USART1 global interrupts.
  */
void USART1_IRQHandler(void);

/**
  * @brief Handle the wakeup button EXTI line.
  */
void EXTI15_10_IRQHandler(void);

/**
  * @brief Handle LPTIM2 interrupts used by the DSP scheduler.
  */
void LPTIM2_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7xx_IT_H */
