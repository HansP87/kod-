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
#include <stdio.h>
#include <stdarg.h>
#include "adc.h"
#include "lptim.h"
#include "stm32h7xx_hal_adc.h"
#include "usart.h"

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;

/* Private define ------------------------------------------------------------*/
#define ADC_CH_PER_ADC          4U
#define ADC_SAMPLES_PER_DSP     2U
#define ADC_BUF_LEN             (ADC_CH_PER_ADC * ADC_SAMPLES_PER_DSP)

#define ADC2_TEMP_INDEX         2U
#define ADC2_VREF_INDEX         3U

#define ADC_TRIGGER_PERIOD      1499U
#define ADC_TRIGGER_PULSE       750U
#define DSP_TICK_PERIOD         2999U
#define DSP_STARTUP_DELAY_MS    1U

#define DSP_FLAG_TICK           (1U << 0)
#define TX_FLAG_SEND           	(1U << 0)

#define DSP_STATUS_OK           0x00000000U
#define DSP_STATUS_ADC_LATE     0x00000001U
#define DSP_STATUS_ADC_MISSED   0x00000002U

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t adc1_frame_count = 0U;
static volatile uint32_t adc2_frame_count = 0U;

static volatile uint32_t adc_overrun_count = 0U;
static volatile uint32_t adc_missed_count  = 0U;

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

static tx_packet_t tx_packet_buffer_a;
static tx_packet_t tx_packet_buffer_b;

static tx_packet_t * volatile published_tx_packet = &tx_packet_buffer_a;
static tx_packet_t * volatile working_tx_packet   = &tx_packet_buffer_b;

static volatile tx_packet_t tx_last_sent_debug;

static uint32_t dsp_sequence = 0U;

__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc1_buf[ADC_BUF_LEN];

__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc2_buf[ADC_BUF_LEN];

/* Definitions for defaultTask */
osThreadId_t default_task_handle;
const osThreadAttr_t default_task_attributes = {
		.name = "default_task",
		.stack_size = 128 * 4,
		.priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for dsp */
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
/* Definitions for transmit */
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

static char tx_message_buffer[512];
/* Definitions for monitor */
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

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Serialize a transmit packet into the UART text buffer.
 * @param pkt Packet to serialize.
 * @param buf Destination character buffer.
 * @param size Size of the destination buffer in bytes.
 * @return Number of bytes written, or `-1` if the buffer is too small.
 */
static int serialize_tx_packet(const tx_packet_t *pkt, char *buf, size_t size);

/**
 * @brief Append formatted text to a partially filled character buffer.
 * @param buf Destination character buffer.
 * @param size Total size of the destination buffer in bytes.
 * @param len Current string length already stored in the buffer.
 * @param format Printf-style format string.
 * @return Updated string length, or `-1` on overflow or formatting error.
 */
static int append_to_buffer(char *buf, size_t size, int len, const char *format, ...);

/**
 * @brief Convert a sample and channel pair into a linear DMA buffer index.
 * @param sample Sample index within the current DSP frame.
 * @param channel ADC channel index within the scan sequence.
 * @return Flattened array index for the DMA buffer.
 */
static uint32_t get_adc_buffer_index(uint32_t sample, uint32_t channel);

/**
 * @brief Estimate VDDA in millivolts from the measured VREFINT sample.
 * @param vref_raw Raw ADC reading for the internal reference channel.
 * @return Supply voltage in millivolts, or `0` if the reference sample is invalid.
 */
static uint32_t calculate_vdda_mv(uint16_t vref_raw);

/**
 * @brief Reset runtime counters used by the DSP pipeline.
 */
static void reset_dsp_runtime_state(void);

/**
 * @brief Start the ADC DMA transfers and timer sources used by the DSP pipeline.
 */
static void start_sampling_pipeline(void);

/**
 * @brief Prepare a fresh transmit packet before filling it with ADC data.
 * @param packet Packet buffer to initialize.
 */
static void prepare_tx_packet(tx_packet_t *packet);

/**
 * @brief Convert the latest DMA frame into engineering units.
 * @param packet Packet buffer that receives the converted values.
 */
static void fill_tx_packet_from_dma(tx_packet_t *packet);

/**
 * @brief Process one DSP scheduling tick and publish a frame if one is ready.
 * @param last_adc1_frame Last ADC1 frame counter consumed by the DSP task.
 * @param last_adc2_frame Last ADC2 frame counter consumed by the DSP task.
 */
static void process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame);

/**
 * @brief Atomically swap the published and working transmit buffers.
 */
static void swap_tx_buffers(void);

static void start_default_task(void *argument);
static void start_dsp_task(void *argument);
static void start_transmit_task(void *argument);
static void start_monitor_task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) {

	/* add mutexes, ... */

	/* add semaphores, ... */

	/* start timers, add new ones, ... */

	/* add queues, ... */

	/* Create the thread(s) */
	/* creation of defaultTask */
	default_task_handle = osThreadNew(start_default_task, NULL, &default_task_attributes);

	/* creation of dsp */
	dsp_task_handle = osThreadNew(start_dsp_task, NULL, &dsp_task_attributes);

	/* creation of transmit */
	transmit_task_handle = osThreadNew(start_transmit_task, NULL, &transmit_task_attributes);

	/* creation of monitor */
	monitor_task_handle = osThreadNew(start_monitor_task, NULL, &monitor_task_attributes);

	/* add threads, ... */

	/* add events, ... */

}

/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
static void start_default_task(void *argument)
{
  for (;;)
  {
    osDelay(1000);
  }
}

/**
 * @brief Function implementing the dsp thread.
 * @param argument: Not used
 * @retval None
 */

static void start_dsp_task(void *argument)
{
	uint32_t flags;

	uint32_t last_adc1_frame = 0U;
	uint32_t last_adc2_frame = 0U;

	(void)argument;

	reset_dsp_runtime_state();
	start_sampling_pipeline();

	for (;;)
	{
		flags = osThreadFlagsWait(DSP_FLAG_TICK, osFlagsWaitAny, osWaitForever);

		if ((flags & DSP_FLAG_TICK) != 0U)
		{
			process_dsp_tick(&last_adc1_frame, &last_adc2_frame);
		}
	}
}

/**
 * @brief Function implementing the transmit thread.
 * @param argument: Not used
 * @retval None
 */
static void start_transmit_task(void *argument)
{
  uint32_t flags;
	tx_packet_t current_tx_packet;
  int length;

	(void)argument;

  for (;;)
  {
    flags = osThreadFlagsWait(TX_FLAG_SEND, osFlagsWaitAny, osWaitForever);

    if ((flags & TX_FLAG_SEND) != 0U)
    {
      taskENTER_CRITICAL();
			current_tx_packet = *published_tx_packet;
      taskEXIT_CRITICAL();

			length = serialize_tx_packet(&current_tx_packet, tx_message_buffer, sizeof(tx_message_buffer));
      if (length > 0)
      {
        HAL_UART_Transmit(&huart1, (uint8_t *)tx_message_buffer, (uint16_t)length, HAL_MAX_DELAY);
      }

			tx_last_sent_debug = current_tx_packet;
    }
  }
}

/**
 * @brief Function implementing the monitor thread.
 * @param argument: Not used
 * @retval None
 */
static void start_monitor_task(void *argument)
{
	(void)argument;

	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
	}
}

/**
 * @brief Atomically publish the latest completed packet buffer.
 */
static void swap_tx_buffers(void)
{
    taskENTER_CRITICAL();
	tx_packet_t *tmp = published_tx_packet;
	published_tx_packet = working_tx_packet;
	working_tx_packet = tmp;
    taskEXIT_CRITICAL();
}

/**
 * @brief Flatten a sample and channel coordinate into the DMA buffer layout.
 * @param sample Sample index within the current DSP frame.
 * @param channel Channel index within the ADC scan sequence.
 * @return Flattened DMA buffer index.
 */
static uint32_t get_adc_buffer_index(uint32_t sample, uint32_t channel)
{
	return (sample * ADC_CH_PER_ADC) + channel;
}

/**
 * @brief Convert the internal VREF sample into an estimated VDDA value.
 * @param vref_raw Raw ADC sample from the VREFINT channel.
 * @return Supply voltage in millivolts, or `0` when the sample is invalid.
 */
static uint32_t calculate_vdda_mv(uint16_t vref_raw)
{
	if (vref_raw == 0U)
	{
		return 0U;
	}

	return ((uint32_t)(*VREFINT_CAL_ADDR) * 3300UL) / (uint32_t)vref_raw;
}

/**
 * @brief Reset DSP counters and packet sequencing state.
 */
static void reset_dsp_runtime_state(void)
{
	dsp_sequence = 0U;
	adc1_frame_count = 0U;
	adc2_frame_count = 0U;
	adc_overrun_count = 0U;
	adc_missed_count = 0U;
}

/**
 * @brief Start the DMA-backed sampling pipeline and its scheduling timers.
 */
static void start_sampling_pipeline(void)
{
	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_buf, ADC_BUF_LEN) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_buf, ADC_BUF_LEN) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_LPTIM_PWM_Start(&hlptim1, ADC_TRIGGER_PERIOD, ADC_TRIGGER_PULSE) != HAL_OK)
	{
		Error_Handler();
	}

	osDelay(DSP_STARTUP_DELAY_MS);

	if (HAL_LPTIM_Counter_Start_IT(&hlptim2, DSP_TICK_PERIOD) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief Initialize metadata for a packet that is about to be filled.
 * @param packet Packet buffer to prepare.
 */
static void prepare_tx_packet(tx_packet_t *packet)
{
	packet->sequence = ++dsp_sequence;
	packet->timestamp = dsp_sequence;
	packet->status_flags = DSP_STATUS_OK;
}

/**
 * @brief Convert the raw ADC DMA buffers into millivolt and temperature values.
 * @param packet Packet buffer that receives the converted results.
 */
static void fill_tx_packet_from_dma(tx_packet_t *packet)
{
	for (uint32_t sample = 0U; sample < ADC_SAMPLES_PER_DSP; sample++)
	{
		uint32_t vref_index = get_adc_buffer_index(sample, ADC2_VREF_INDEX);
		uint32_t temp_index = get_adc_buffer_index(sample, ADC2_TEMP_INDEX);

		packet->vdda_mv[sample] = calculate_vdda_mv(adc2_buf[vref_index]);
		if (packet->vdda_mv[sample] == 0U)
		{
			packet->status_flags |= DSP_STATUS_ADC_LATE;
		}

		for (uint32_t channel = 0U; channel < ADC_CH_PER_ADC; channel++)
		{
			uint32_t adc_index = get_adc_buffer_index(sample, channel);

			packet->adc1_mv[sample][channel] = __HAL_ADC_CALC_DATA_TO_VOLTAGE(
					packet->vdda_mv[sample],
					adc1_buf[adc_index],
					ADC_RESOLUTION_16B);

			packet->adc2_mv[sample][channel] = __HAL_ADC_CALC_DATA_TO_VOLTAGE(
					packet->vdda_mv[sample],
					adc2_buf[adc_index],
					ADC_RESOLUTION_16B);
		}

		packet->temperature_c[sample] = __HAL_ADC_CALC_TEMPERATURE(
				packet->vdda_mv[sample],
				adc2_buf[temp_index],
				ADC_RESOLUTION_16B);
	}
}

/**
 * @brief Consume the latest ADC frame counts and publish a packet when valid.
 * @param last_adc1_frame Last ADC1 frame counter consumed by the DSP task.
 * @param last_adc2_frame Last ADC2 frame counter consumed by the DSP task.
 */
static void process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame)
{
	uint32_t adc1_now = adc1_frame_count;
	uint32_t adc2_now = adc2_frame_count;
	uint32_t adc1_delta = adc1_now - *last_adc1_frame;
	uint32_t adc2_delta = adc2_now - *last_adc2_frame;

	prepare_tx_packet(working_tx_packet);

	if ((adc1_delta == 1U) && (adc2_delta == 1U))
	{
		fill_tx_packet_from_dma(working_tx_packet);
		*last_adc1_frame = adc1_now;
		*last_adc2_frame = adc2_now;
		swap_tx_buffers();
		return;
	}

	if ((adc1_delta == 0U) || (adc2_delta == 0U))
	{
		adc_overrun_count++;
		working_tx_packet->status_flags |= DSP_STATUS_ADC_LATE;
		return;
	}

	adc_missed_count++;
	*last_adc1_frame = adc1_now;
	*last_adc2_frame = adc2_now;
	working_tx_packet->status_flags |= DSP_STATUS_ADC_MISSED;
}

/**
 * @brief Append formatted data to an output buffer with bounds checking.
 * @param buf Destination character buffer.
 * @param size Total size of the destination buffer.
 * @param len Current number of valid bytes already present.
 * @param format Printf-style format string.
 * @return Updated string length, or `-1` if the write would overflow.
 */
static int append_to_buffer(char *buf, size_t size, int len, const char *format, ...)
{
	int written;
	va_list args;

	if ((len < 0) || ((size_t)len >= size))
	{
		return -1;
	}

	va_start(args, format);
	written = vsnprintf(buf + len, size - (size_t)len, format, args);
	va_end(args);

	if ((written < 0) || ((size_t)written >= (size - (size_t)len)))
	{
		return -1;
	}

	return len + written;
}

/**
 * @brief Format one transmit packet as a human-readable UART message.
 * @param pkt Packet to serialize.
 * @param buf Destination character buffer.
 * @param size Total size of the destination buffer.
 * @return Number of bytes written, or `-1` if the buffer is too small.
 */
static int serialize_tx_packet(const tx_packet_t *pkt, char *buf, size_t size)
{
	int len = 0;

	len = append_to_buffer(buf, size, len,
			"SEQ:%lu TS:%lu FLAGS:0x%08lX\n",
			(unsigned long)pkt->sequence,
			(unsigned long)pkt->timestamp,
			(unsigned long)pkt->status_flags);

	for (uint32_t sample = 0U; (sample < ADC_SAMPLES_PER_DSP) && (len >= 0); sample++)
	{
		len = append_to_buffer(buf, size, len,
				"SAMPLE %u: VDDA=%lu mV TEMP=%ld C ADC1=",
				(unsigned int)sample,
				(unsigned long)pkt->vdda_mv[sample],
				(long)pkt->temperature_c[sample]);

		for (uint32_t channel = 0U; (channel < ADC_CH_PER_ADC) && (len >= 0); channel++)
		{
			len = append_to_buffer(buf, size, len,
					"%lu%s",
					(unsigned long)pkt->adc1_mv[sample][channel],
					(channel + 1U < ADC_CH_PER_ADC) ? "," : " ");
		}

		len = append_to_buffer(buf, size, len, "ADC2=");

		for (uint32_t channel = 0U; (channel < ADC_CH_PER_ADC) && (len >= 0); channel++)
		{
			len = append_to_buffer(buf, size, len,
					"%lu%s",
					(unsigned long)pkt->adc2_mv[sample][channel],
					(channel + 1U < ADC_CH_PER_ADC) ? "," : "\n");
		}
	}

	return len;
}

/* Private application code --------------------------------------------------*/
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
 * @brief Count completed ADC DMA frames.
 * @param hadc ADC instance that completed its conversion sequence.
 */
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

/**
 * @brief Trigger packet transmission when the user wakeup button is pressed.
 * @param GPIO_Pin GPIO pin number associated with the EXTI event.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == WAKEUP_Pin)
    {
		osThreadFlagsSet(transmit_task_handle, TX_FLAG_SEND);
    }
}

