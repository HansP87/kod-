/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
#include "app_runtime.h"
#include "default_task.h"
#include "dsp_task.h"
#include "monitor_task.h"
#include "tim.h"
#include "transmit_task.h"

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;

/* Private variables ---------------------------------------------------------*/
osThreadId_t default_task_handle;
const osThreadAttr_t default_task_attributes = {
		.name = "default_task",
		.stack_size = 128 * 4,
		.priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t dsp_task_handle;
uint32_t dsp_stack_buffer[256];
osStaticThreadDef_t dsp_control_block;
const osThreadAttr_t dsp_task_attributes = {
		.name = "dsp_task",
		.cb_mem = &dsp_control_block,
		.cb_size = sizeof(dsp_control_block),
		.stack_mem = &dsp_stack_buffer[0],
		.stack_size = sizeof(dsp_stack_buffer),
		.priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t transmit_task_handle;
uint32_t transmit_stack_buffer[512];
osStaticThreadDef_t transmit_control_block;
const osThreadAttr_t transmit_task_attributes = {
		.name = "transmit_task",
		.cb_mem = &transmit_control_block,
		.cb_size = sizeof(transmit_control_block),
		.stack_mem = &transmit_stack_buffer[0],
		.stack_size = sizeof(transmit_stack_buffer),
		.priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t monitor_task_handle;
uint32_t monitor_stack_buffer[256];
osStaticThreadDef_t monitor_control_block;
const osThreadAttr_t monitor_task_attributes = {
		.name = "monitor_task",
		.cb_mem = &monitor_control_block,
		.cb_size = sizeof(monitor_control_block),
		.stack_mem = &monitor_stack_buffer[0],
		.stack_size = sizeof(monitor_stack_buffer),
		.priority = (osPriority_t) osPriorityLow,
};

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief FreeRTOS initialization.
 */
void MX_FREERTOS_Init(void)
{
	default_task_handle = osThreadNew(start_default_task, NULL, &default_task_attributes);
	dsp_task_handle = osThreadNew(start_dsp_task, NULL, &dsp_task_attributes);
	transmit_task_handle = osThreadNew(start_transmit_task, NULL, &transmit_task_attributes);
	monitor_task_handle = osThreadNew(start_monitor_task, NULL, &monitor_task_attributes);
}

/**
 * @brief Wake the DSP task each time the LPTIM2 scheduler period elapses.
 * @param hlptim Timer instance that raised the callback.
 */
void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
	if (hlptim->Instance == LPTIM2)
	{
		osThreadFlagsSet(dsp_task_handle, DSP_FLAG_TICK);
	}
}

/**
 * @brief Record completed ADC DMA frames.
 * @param hadc ADC instance that completed its conversion sequence.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	app_runtime_record_adc_frame_ready(hadc);
}

/**
 * @brief Trigger packet transmission when the user wakeup button is pressed.
 * @param GPIO_Pin GPIO pin number associated with the EXTI event.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == WAKEUP_Pin)
	{
		app_runtime_set_button_event_timestamp_us(tim2_get_timestamp_us());
		osThreadFlagsSet(transmit_task_handle, TX_FLAG_SEND);
	}
}
