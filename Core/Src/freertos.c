/* USER CODE BEGIN Header */
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for dsp */
osThreadId_t dspHandle;
uint32_t dspBuffer[ 256 ];
osStaticThreadDef_t dspControlBlock;
const osThreadAttr_t dsp_attributes = {
  .name = "dsp",
  .cb_mem = &dspControlBlock,
  .cb_size = sizeof(dspControlBlock),
  .stack_mem = &dspBuffer[0],
  .stack_size = sizeof(dspBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for transmit */
osThreadId_t transmitHandle;
uint32_t transmitBuffer[ 256 ];
osStaticThreadDef_t transmitControlBlock;
const osThreadAttr_t transmit_attributes = {
  .name = "transmit",
  .cb_mem = &transmitControlBlock,
  .cb_size = sizeof(transmitControlBlock),
  .stack_mem = &transmitBuffer[0],
  .stack_size = sizeof(transmitBuffer),
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for monitor */
osThreadId_t monitorHandle;
uint32_t monitorBuffer[ 256 ];
osStaticThreadDef_t monitorControlBlock;
const osThreadAttr_t monitor_attributes = {
  .name = "monitor",
  .cb_mem = &monitorControlBlock,
  .cb_size = sizeof(monitorControlBlock),
  .stack_mem = &monitorBuffer[0],
  .stack_size = sizeof(monitorBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for transmitQue */
osMessageQueueId_t transmitQueHandle;
uint8_t transmitQueBuffer[ 6 * sizeof( uint16_t ) ];
osStaticMessageQDef_t transmitQueControlBlock;
const osMessageQueueAttr_t transmitQue_attributes = {
  .name = "transmitQue",
  .cb_mem = &transmitQueControlBlock,
  .cb_size = sizeof(transmitQueControlBlock),
  .mq_mem = &transmitQueBuffer,
  .mq_size = sizeof(transmitQueBuffer)
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartDSP(void *argument);
void transmitStart(void *argument);
void startMonitor(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of transmitQue */
  transmitQueHandle = osMessageQueueNew (6, sizeof(uint16_t), &transmitQue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of dsp */
  dspHandle = osThreadNew(StartDSP, NULL, &dsp_attributes);

  /* creation of transmit */
  transmitHandle = osThreadNew(transmitStart, NULL, &transmit_attributes);

  /* creation of monitor */
  monitorHandle = osThreadNew(startMonitor, NULL, &monitor_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartDSP */
/**
* @brief Function implementing the dsp thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDSP */
void StartDSP(void *argument)
{
  /* USER CODE BEGIN StartDSP */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDSP */
}

/* USER CODE BEGIN Header_transmitStart */
/**
* @brief Function implementing the transmit thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_transmitStart */
void transmitStart(void *argument)
{
  /* USER CODE BEGIN transmitStart */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END transmitStart */
}

/* USER CODE BEGIN Header_startMonitor */
/**
* @brief Function implementing the monitor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startMonitor */
void startMonitor(void *argument)
{
  /* USER CODE BEGIN startMonitor */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END startMonitor */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

