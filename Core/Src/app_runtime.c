#include <stdio.h>
#include <stdarg.h>
#include "cmsis_os2.h"
#include "main.h"
#include "adc.h"
#include "lptim.h"
#include "stm32h7xx_hal_adc.h"
#include "tim.h"
#include "app_runtime.h"

#define ADC_BUF_LEN             (ADC_CH_PER_ADC * ADC_SAMPLES_PER_DSP)
#define ADC2_TEMP_INDEX         2U
#define ADC2_VREF_INDEX         3U

#define ADC_TRIGGER_PERIOD      1499U
#define ADC_TRIGGER_PULSE       750U
#define DSP_TICK_PERIOD         2999U
#define DSP_STARTUP_DELAY_MS    1U
#define TX_RETURN_QUEUE_LEN     3U

#define DSP_STATUS_OK           0x00000000U
#define DSP_STATUS_ADC_LATE     0x00000001U
#define DSP_STATUS_ADC_MISSED   0x00000002U

static volatile uint32_t adc1_frame_count = 0U;
static volatile uint32_t adc2_frame_count = 0U;
static uint32_t adc1_ready_timestamp_us = 0U;
static uint32_t adc2_ready_timestamp_us = 0U;
static uint32_t button_event_timestamp_us = 0U;

static volatile uint32_t adc_overrun_count = 0U;
static volatile uint32_t adc_missed_count = 0U;

static tx_packet_t tx_packet_buffer_a;
static tx_packet_t tx_packet_buffer_b;
static tx_packet_t tx_packet_buffer_c;

static tx_packet_t *working_tx_packet = &tx_packet_buffer_a;
static tx_packet_t *free_tx_packet_stack[2] = {
		&tx_packet_buffer_b,
		&tx_packet_buffer_c,
};
static uint32_t free_tx_packet_count = 2U;

static tx_packet_t *ready_tx_packet = NULL;
static tx_packet_t *returned_tx_packet_queue[TX_RETURN_QUEUE_LEN];
static uint32_t returned_tx_packet_write_index = 0U;
static uint32_t returned_tx_packet_read_index = 0U;

static volatile tx_packet_t tx_last_sent_debug;
static uint32_t dsp_sequence = 0U;
static uint32_t app_mode_state = APP_MODE_STREAMING;

__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc1_buf[ADC_BUF_LEN];
__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc2_buf[ADC_BUF_LEN];

/** Append formatted text to the packet serialization buffer. */
static int append_to_buffer(char *buf, size_t size, int len, const char *format, ...);
/** Convert a sample/channel pair into the linear DMA buffer index. */
static uint32_t get_adc_buffer_index(uint32_t sample, uint32_t channel);
/** Convert the internal reference measurement into the current VDDA estimate. */
static uint32_t calculate_vdda_mv(uint16_t vref_raw);
/** Initialize a packet header before the latest samples are copied in. */
static void prepare_tx_packet(tx_packet_t *packet);
/** Fill a packet structure from the current ADC DMA buffers. */
static void fill_tx_packet_from_dma(tx_packet_t *packet);
/** Reset the transmit buffer ownership state to its startup configuration. */
static void initialize_tx_buffer_ownership(void);
/** Swap the current ready packet atomically. */
static tx_packet_t *exchange_ready_tx_packet(tx_packet_t *packet);
/** Load the write index for the returned-packet queue. */
static uint32_t load_returned_tx_write_index(void);
/** Load the read index for the returned-packet queue. */
static uint32_t load_returned_tx_read_index(void);
/** Store the write index for the returned-packet queue. */
static void store_returned_tx_write_index(uint32_t write_index);
/** Store the read index for the returned-packet queue. */
static void store_returned_tx_read_index(uint32_t read_index);
/** Load the latest trigger timestamp used for packet age reporting. */
static uint32_t load_button_event_timestamp_us(void);
/** Store the latest ADC1 frame-ready timestamp. */
static void store_adc1_ready_timestamp_us(uint32_t timestamp_us);
/** Store the latest ADC2 frame-ready timestamp. */
static void store_adc2_ready_timestamp_us(uint32_t timestamp_us);
/** Select the newest ADC-ready timestamp between both converters. */
static uint32_t get_latest_sample_ready_timestamp_us(void);
/** Publish the working packet and acquire a new free packet buffer. */
static void publish_working_tx_packet(void);
/** Return a packet buffer to the free stack. */
static void push_free_tx_packet(tx_packet_t *packet);
/** Pop a free packet buffer from the free stack. */
static tx_packet_t *pop_free_tx_packet(void);
/** Move transmit buffers returned by the TX task back to the free stack. */
static void reclaim_returned_tx_packets(void);
/** Queue a packet buffer to be reclaimed by the DSP task later. */
static int queue_returned_tx_packet(tx_packet_t *packet);

void app_runtime_reset(void)
{
	dsp_sequence = 0U;
	adc1_frame_count = 0U;
	adc2_frame_count = 0U;
	adc_overrun_count = 0U;
	adc_missed_count = 0U;
	app_runtime_set_mode(APP_MODE_STREAMING);
}

/**
 * @brief Initialize the transmit buffer ownership model and timestamp state.
 */
void app_runtime_initialize_tx_buffer_ownership(void)
{
	initialize_tx_buffer_ownership();
	app_runtime_set_button_event_timestamp_us(0U);
	store_adc1_ready_timestamp_us(0U);
	store_adc2_ready_timestamp_us(0U);
}

/**
 * @brief Start the ADC DMA engines and timer sources that drive sampling.
 */
void app_runtime_start_sampling_pipeline(void)
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
 * @brief Process one DSP scheduling tick and publish a packet when both ADCs are ready.
 * @param last_adc1_frame Last ADC1 frame counter processed by the caller.
 * @param last_adc2_frame Last ADC2 frame counter processed by the caller.
 */
void app_runtime_process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame)
{
	uint32_t adc1_now = adc1_frame_count;
	uint32_t adc2_now = adc2_frame_count;
	uint32_t adc1_delta = adc1_now - *last_adc1_frame;
	uint32_t adc2_delta = adc2_now - *last_adc2_frame;

	reclaim_returned_tx_packets();
	prepare_tx_packet(working_tx_packet);

	if ((adc1_delta == 1U) && (adc2_delta == 1U))
	{
		fill_tx_packet_from_dma(working_tx_packet);
		working_tx_packet->sample_ready_timestamp_us = get_latest_sample_ready_timestamp_us();
		*last_adc1_frame = adc1_now;
		*last_adc2_frame = adc2_now;
		publish_working_tx_packet();
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
 * @brief Hand the newest ready packet to the TX task and queue the old one for reclamation.
 * @param current_tx_packet Pointer to the caller-owned packet pointer.
 */
void app_runtime_claim_latest_tx_packet(tx_packet_t **current_tx_packet)
{
	tx_packet_t *ready_packet = exchange_ready_tx_packet(NULL);

	if (ready_packet == NULL)
	{
		return;
	}

	if ((*current_tx_packet != NULL) && (queue_returned_tx_packet(*current_tx_packet) != 0))
	{
		Error_Handler();
	}

	*current_tx_packet = ready_packet;
}

/**
 * @brief Compute packet age relative to the latest trigger timestamp.
 * @param packet Packet whose sample-ready timestamp should be compared.
 * @return Latency in microseconds.
 */
uint32_t app_runtime_get_sample_latency_us(const tx_packet_t *packet)
{
	return load_button_event_timestamp_us() - packet->sample_ready_timestamp_us;
}

/**
 * @brief Serialize a packet into the line-oriented UART reporting format.
 * @param pkt Packet to serialize.
 * @param sample_latency_us Packet age to report in the header line.
 * @param buf Destination buffer for the ASCII output.
 * @param size Capacity of @p buf in bytes.
 * @return Number of bytes written, or `-1` if the output does not fit.
 */
int app_runtime_serialize_tx_packet(const tx_packet_t *pkt, uint32_t sample_latency_us, char *buf, size_t size)
{
	int len = 0;

	len = append_to_buffer(buf, size, len,
			"SEQ:%lu TS:%lu READY_US:%lu AGE_US:%lu FLAGS:0x%08lX\n",
			(unsigned long)pkt->sequence,
			(unsigned long)pkt->timestamp,
			(unsigned long)pkt->sample_ready_timestamp_us,
			(unsigned long)sample_latency_us,
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

/**
 * @brief Store the timestamp of the latest trigger event.
 * @param timestamp_us Trigger timestamp in microseconds.
 */
void app_runtime_set_button_event_timestamp_us(uint32_t timestamp_us)
{
	__atomic_store_n(&button_event_timestamp_us, timestamp_us, __ATOMIC_RELEASE);
}

/**
 * @brief Record that one ADC DMA frame completed and capture its completion timestamp.
 * @param hadc ADC instance that raised the completion callback.
 */
void app_runtime_record_adc_frame_ready(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		store_adc1_ready_timestamp_us(tim2_get_timestamp_us());
		adc1_frame_count++;
	}
	else if (hadc->Instance == ADC2)
	{
		store_adc2_ready_timestamp_us(tim2_get_timestamp_us());
		adc2_frame_count++;
	}
}

/**
 * @brief Copy the last transmitted packet into a debug snapshot.
 * @param packet Packet that was transmitted most recently.
 */
void app_runtime_store_last_sent_debug(const tx_packet_t *packet)
{
	tx_last_sent_debug = *packet;
}

/**
 * @brief Update the application streaming/configuration mode.
 * @param mode New mode value.
 */
void app_runtime_set_mode(app_mode_t mode)
{
	__atomic_store_n(&app_mode_state, (uint32_t)mode, __ATOMIC_RELEASE);
}

/**
 * @brief Read the current application mode.
 * @return Current mode value.
 */
app_mode_t app_runtime_get_mode(void)
{
	return (app_mode_t)__atomic_load_n(&app_mode_state, __ATOMIC_ACQUIRE);
}

/**
 * @brief Check whether streaming mode is currently active.
 * @return Non-zero in streaming mode, otherwise zero.
 */
uint32_t app_runtime_is_streaming_mode(void)
{
	return (uint32_t)(app_runtime_get_mode() == APP_MODE_STREAMING);
}

/**
 * @brief Append formatted text to a bounded output buffer.
 * @param buf Destination buffer.
 * @param size Capacity of @p buf.
 * @param len Current write length.
 * @param format `printf`-style format string.
 * @return Updated length, or `-1` if the output would overflow.
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
 * @brief Convert a sample/channel pair into a flat DMA buffer index.
 * @param sample Sample number within the current DSP packet.
 * @param channel ADC channel index within the sample.
 * @return Linear index into the DMA buffer.
 */
static uint32_t get_adc_buffer_index(uint32_t sample, uint32_t channel)
{
	return (sample * ADC_CH_PER_ADC) + channel;
}

/**
 * @brief Estimate VDDA from the sampled internal reference voltage.
 * @param vref_raw Raw VREFINT ADC sample.
 * @return Estimated VDDA in millivolts.
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
 * @brief Initialize the packet header and status fields for a new DSP cycle.
 * @param packet Packet buffer being prepared.
 */
static void prepare_tx_packet(tx_packet_t *packet)
{
	packet->sequence = ++dsp_sequence;
	packet->timestamp = dsp_sequence;
	packet->sample_ready_timestamp_us = 0U;
	packet->status_flags = DSP_STATUS_OK;
}

/**
 * @brief Populate a packet from the latest ADC DMA buffers.
 * @param packet Packet buffer to populate.
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
 * @brief Reset packet-buffer ownership to the startup configuration.
 */
static void initialize_tx_buffer_ownership(void)
{
	working_tx_packet = &tx_packet_buffer_a;
	free_tx_packet_stack[0] = &tx_packet_buffer_b;
	free_tx_packet_stack[1] = &tx_packet_buffer_c;
	free_tx_packet_count = 2U;
	exchange_ready_tx_packet(NULL);
	store_returned_tx_write_index(0U);
	store_returned_tx_read_index(0U);
}

/**
 * @brief Atomically replace the currently published ready packet.
 * @param packet Packet to publish, or `NULL` to just take ownership of the current ready packet.
 * @return Previously published packet, or `NULL` if none was ready.
 */
static tx_packet_t *exchange_ready_tx_packet(tx_packet_t *packet)
{
	return __atomic_exchange_n(&ready_tx_packet, packet, __ATOMIC_ACQ_REL);
}

/** @brief Load the returned-packet queue write index. */
static uint32_t load_returned_tx_write_index(void)
{
	return __atomic_load_n(&returned_tx_packet_write_index, __ATOMIC_ACQUIRE);
}

/** @brief Load the returned-packet queue read index. */
static uint32_t load_returned_tx_read_index(void)
{
	return __atomic_load_n(&returned_tx_packet_read_index, __ATOMIC_RELAXED);
}

/**
 * @brief Store the returned-packet queue write index.
 * @param write_index New write index value.
 */
static void store_returned_tx_write_index(uint32_t write_index)
{
	__atomic_store_n(&returned_tx_packet_write_index, write_index, __ATOMIC_RELEASE);
}

/**
 * @brief Store the returned-packet queue read index.
 * @param read_index New read index value.
 */
static void store_returned_tx_read_index(uint32_t read_index)
{
	__atomic_store_n(&returned_tx_packet_read_index, read_index, __ATOMIC_RELEASE);
}

/**
 * @brief Load the latest trigger timestamp used for packet age reporting.
 * @return Trigger timestamp in microseconds.
 */
static uint32_t load_button_event_timestamp_us(void)
{
	return __atomic_load_n(&button_event_timestamp_us, __ATOMIC_ACQUIRE);
}

/**
 * @brief Store the latest ADC1 frame-ready timestamp.
 * @param timestamp_us Timestamp in microseconds.
 */
static void store_adc1_ready_timestamp_us(uint32_t timestamp_us)
{
	__atomic_store_n(&adc1_ready_timestamp_us, timestamp_us, __ATOMIC_RELEASE);
}

/**
 * @brief Store the latest ADC2 frame-ready timestamp.
 * @param timestamp_us Timestamp in microseconds.
 */
static void store_adc2_ready_timestamp_us(uint32_t timestamp_us)
{
	__atomic_store_n(&adc2_ready_timestamp_us, timestamp_us, __ATOMIC_RELEASE);
}

/**
 * @brief Choose the newest ADC completion timestamp across both converters.
 * @return Most recent ADC-ready timestamp in microseconds.
 */
static uint32_t get_latest_sample_ready_timestamp_us(void)
{
	uint32_t adc1_timestamp_us = __atomic_load_n(&adc1_ready_timestamp_us, __ATOMIC_ACQUIRE);
	uint32_t adc2_timestamp_us = __atomic_load_n(&adc2_ready_timestamp_us, __ATOMIC_ACQUIRE);

	if (adc1_timestamp_us >= adc2_timestamp_us)
	{
		return adc1_timestamp_us;
	}

	return adc2_timestamp_us;
}

/**
 * @brief Publish the current working packet and acquire a fresh working buffer.
 */
static void publish_working_tx_packet(void)
{
	tx_packet_t *stale_ready_packet = exchange_ready_tx_packet(working_tx_packet);
	tx_packet_t *next_working_packet;

	if (stale_ready_packet != NULL)
	{
		push_free_tx_packet(stale_ready_packet);
	}

	next_working_packet = pop_free_tx_packet();
	if (next_working_packet == NULL)
	{
		Error_Handler();
	}

	working_tx_packet = next_working_packet;
}

/**
 * @brief Return a packet buffer to the free stack.
 * @param packet Packet buffer to recycle.
 */
static void push_free_tx_packet(tx_packet_t *packet)
{
	if ((packet == NULL) || (free_tx_packet_count >= 2U))
	{
		Error_Handler();
	}

	free_tx_packet_stack[free_tx_packet_count] = packet;
	free_tx_packet_count++;
}

/**
 * @brief Pop the next available free packet buffer.
 * @return Free packet buffer, or `NULL` if none are available.
 */
static tx_packet_t *pop_free_tx_packet(void)
{
	if (free_tx_packet_count == 0U)
	{
		return NULL;
	}

	free_tx_packet_count--;
	return free_tx_packet_stack[free_tx_packet_count];
}

/**
 * @brief Reclaim packet buffers that were returned by the transmit task.
 */
static void reclaim_returned_tx_packets(void)
{
	uint32_t read_index = load_returned_tx_read_index();
	uint32_t write_index = load_returned_tx_write_index();

	while (read_index != write_index)
	{
		push_free_tx_packet(returned_tx_packet_queue[read_index]);
		read_index = (read_index + 1U) % TX_RETURN_QUEUE_LEN;
	}

	store_returned_tx_read_index(read_index);
}

/**
 * @brief Queue a transmitted packet for later reclamation by the DSP task.
 * @param packet Packet buffer to return.
 * @return Zero on success, or `-1` if the return queue is full.
 */
static int queue_returned_tx_packet(tx_packet_t *packet)
{
	uint32_t write_index = returned_tx_packet_write_index;
	uint32_t read_index = load_returned_tx_read_index();
	uint32_t next_write_index = (write_index + 1U) % TX_RETURN_QUEUE_LEN;

	if (next_write_index == read_index)
	{
		return -1;
	}

	returned_tx_packet_queue[write_index] = packet;
	store_returned_tx_write_index(next_write_index);
	return 0;
}
