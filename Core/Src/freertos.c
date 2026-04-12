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
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "adc.h"
#include "lptim.h"
#include "stm32h7xx_hal_adc.h"
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_CH_PER_ADC          4U
#define ADC_SAMPLES_PER_DSP     2U
#define ADC_BUF_LEN             (ADC_CH_PER_ADC * ADC_SAMPLES_PER_DSP)

#define DSP_FLAG_TICK           (1U << 0)
#define TX_FLAG_SEND           	(1U << 0)

#define DSP_STATUS_OK           0x00000000U
#define DSP_STATUS_ADC_LATE     0x00000001U
#define DSP_STATUS_ADC_MISSED   0x00000002U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint32_t adc1_frame_count = 0U;
volatile uint32_t adc2_frame_count = 0U;

volatile uint32_t adc_overrun_count = 0U;
volatile uint32_t adc_missed_count  = 0U;

typedef struct
{
	uint32_t sequence;
	uint32_t timestamp;

	uint32_t vdda_mv[ADC_SAMPLES_PER_DSP];
	int32_t  temperature_c[ADC_SAMPLES_PER_DSP];

	uint32_t adc1_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];
	uint32_t adc2_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];

	uint32_t status_flags;
} tx_packet_t;

static tx_packet_t tx_buf_a;
static tx_packet_t tx_buf_b;

static tx_packet_t * volatile tx_published = &tx_buf_a;
static tx_packet_t * volatile tx_working   = &tx_buf_b;

volatile tx_packet_t tx_last_sent_debug;

static uint32_t dsp_sequence = 0U;

typedef struct
{
	uint16_t adc1_raw[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];
	uint16_t adc2_raw[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];

	uint32_t vdda_mv[ADC_SAMPLES_PER_DSP];

	uint32_t adc1_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];
	uint32_t adc2_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];

	int32_t temperature_c[ADC_SAMPLES_PER_DSP];
} dsp_frame_t;

typedef struct
{
	uint32_t adc1_frame_count;
	uint32_t adc2_frame_count;
	uint32_t adc_overrun_count;
	uint32_t adc_missed_count;
	uint8_t  frame_valid;
} dsp_status_t;

__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc1_buf[ADC_BUF_LEN];

__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc2_buf[ADC_BUF_LEN];


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

static int serialize_tx_packet(const tx_packet_t *pkt, char *buf, size_t size);
static void swap_tx_buffers(void);

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
  for (;;)
  {
    osThreadFlagsSet(transmitHandle, TX_FLAG_SEND);
    osDelay(1000);
  }
}

/* USER CODE BEGIN Header_StartDSP */
/**
 * @brief Function implementing the dsp thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDSP */

/* USER CODE BEGIN StartDSP */
void StartDSP(void *argument)
{
	uint32_t flags;

	uint32_t last_adc1_frame = 0U;
	uint32_t last_adc2_frame = 0U;

	dsp_sequence = 0U;

	adc1_frame_count = 0U;
	adc2_frame_count = 0U;
	adc_overrun_count = 0U;
	adc_missed_count  = 0U;

	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_buf, ADC_BUF_LEN) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_buf, ADC_BUF_LEN) != HAL_OK)
	{
		Error_Handler();
	}

	/* Start ADC trigger timer first */
	if (HAL_LPTIM_PWM_Start(&hlptim1, 1499, 750) != HAL_OK)
	{
		Error_Handler();
	}

	/* Optional small startup guard so DSP tick is phase-shifted later than ADC frame completion */
	osDelay(1);

	/* Start DSP scheduler timer */
	if (HAL_LPTIM_Counter_Start_IT(&hlptim2, 2999) != HAL_OK)
	{
		Error_Handler();
	}

	for (;;)
	{
		flags = osThreadFlagsWait(DSP_FLAG_TICK, osFlagsWaitAny, osWaitForever);

		if ((flags & DSP_FLAG_TICK) != 0U)
		{
			uint32_t adc1_now = adc1_frame_count;
			uint32_t adc2_now = adc2_frame_count;

			uint32_t adc1_delta = adc1_now - last_adc1_frame;
			uint32_t adc2_delta = adc2_now - last_adc2_frame;

			tx_working->sequence = ++dsp_sequence;
			tx_working->timestamp = dsp_sequence;   /* temporary placeholder */
			tx_working->status_flags = DSP_STATUS_OK;

			if ((adc1_delta == 1U) && (adc2_delta == 1U))
			{
				for (uint32_t s = 0; s < ADC_SAMPLES_PER_DSP; s++)
				{
					uint32_t vref_cal = (uint32_t)(*VREFINT_CAL_ADDR);
					uint32_t vref_raw = (uint32_t)adc2_buf[s * ADC_CH_PER_ADC + 3U];

					if (vref_raw != 0U)
					{
					  tx_working->vdda_mv[s] = (vref_cal * 3300UL) / vref_raw;
					}
					else
					{
					  tx_working->vdda_mv[s] = 0U;
					  tx_working->status_flags |= DSP_STATUS_ADC_LATE;
					}

					for (uint32_t ch = 0; ch < ADC_CH_PER_ADC; ch++)
					{
						uint32_t adc1_raw = adc1_buf[s * ADC_CH_PER_ADC + ch];
						uint32_t adc2_raw = adc2_buf[s * ADC_CH_PER_ADC + ch];

						tx_working->adc1_mv[s][ch] =
								__HAL_ADC_CALC_DATA_TO_VOLTAGE(tx_working->vdda_mv[s],
										adc1_raw,
										ADC_RESOLUTION_16B);

						tx_working->adc2_mv[s][ch] =
								__HAL_ADC_CALC_DATA_TO_VOLTAGE(tx_working->vdda_mv[s],
										adc2_raw,
										ADC_RESOLUTION_16B);
					}

					tx_working->temperature_c[s] =
							__HAL_ADC_CALC_TEMPERATURE(tx_working->vdda_mv[s],
									adc2_buf[s * ADC_CH_PER_ADC + 2U],
									ADC_RESOLUTION_16B);
				}

				last_adc1_frame = adc1_now;
				last_adc2_frame = adc2_now;

				/* later: send frame to transmit thread */
			}
			else if ((adc1_delta == 0U) || (adc2_delta == 0U))
			{
				adc_overrun_count++;
				tx_working->status_flags |= DSP_STATUS_ADC_LATE;
			}
			else
			{
				adc_missed_count++;
				last_adc1_frame = adc1_now;
				last_adc2_frame = adc2_now;
			}
		}
	}
}
/* USER CODE END StartDSP */

/* USER CODE BEGIN Header_transmitStart */
/**
 * @brief Function implementing the transmit thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_transmitStart */
void transmitStart(void *argument)
{
  uint32_t flags;
  char tx_buffer[512];
  tx_packet_t current_packet;
  int length;

  for (;;)
  {
    flags = osThreadFlagsWait(TX_FLAG_SEND, osFlagsWaitAny, osWaitForever);

    if ((flags & TX_FLAG_SEND) != 0U)
    {
      taskENTER_CRITICAL();
      current_packet = *tx_published;
      taskEXIT_CRITICAL();

      length = serialize_tx_packet(&current_packet, tx_buffer, sizeof(tx_buffer));
      if (length > 0)
      {
        HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, (uint16_t)length, HAL_MAX_DELAY);
      }

      tx_last_sent_debug = current_packet;
    }
  }
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
void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
	if (hlptim->Instance == LPTIM2)
	{
		osThreadFlagsSet(dspHandle, DSP_FLAG_TICK);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		adc1_frame_count++;
	}
	else if (hadc->Instance == ADC2)
	{
		adc2_frame_count++;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == WAKEUP_Pin)
    {
        osThreadFlagsSet(transmitHandle, TX_FLAG_SEND);
    }
}
/* USER CODE END Application */

