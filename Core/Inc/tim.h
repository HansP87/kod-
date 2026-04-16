/**
  ******************************************************************************
  * @file    tim.h
  * @brief   This file contains all the function prototypes for
  *          the tim.c file
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
#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

extern TIM_HandleTypeDef htim2;

/**
  * @brief Initialize TIM2 as the free-running timing base.
  */
void MX_TIM2_Init(void);

/**
  * @brief Read the current TIM2 counter value in microseconds.
  * @return Free-running TIM2 timestamp in microseconds.
  */
uint32_t tim2_get_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIM_H__ */

