/**
 * @file tx_packet_service.c
 * @brief Packet ownership rotation and compact normal-stream serialization.
 * @ingroup transport_services
 */

#include <string.h>
#include "app_mode_service.h"
#include "main.h"
#include "tim.h"
#include "tx_packet_service.h"

#define TX_RETURN_QUEUE_LEN     3U

typedef struct __attribute__((packed))
{
	uint32_t age_us;
	uint32_t status_flags;
	uint16_t filtered_vdda_mv;
	int16_t filtered_temperature_c;
	uint16_t filtered_adc1_mv[ADC_CH_PER_ADC];
	uint16_t filtered_adc2_mv[ADC_CH_PER_ADC];
} tx_stream_payload_t;

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

/** @brief Atomically swap the currently published ready packet. */
static tx_packet_t *exchange_ready_tx_packet(tx_packet_t *packet);
/** @brief Load the write index of the returned-packet ring buffer. */
static uint32_t load_returned_tx_write_index(void);
/** @brief Load the read index of the returned-packet ring buffer. */
static uint32_t load_returned_tx_read_index(void);
/** @brief Store the write index of the returned-packet ring buffer. */
static void store_returned_tx_write_index(uint32_t write_index);
/** @brief Store the read index of the returned-packet ring buffer. */
static void store_returned_tx_read_index(uint32_t read_index);
/** @brief Push a free packet back onto the local free-stack. */
static void push_free_tx_packet(tx_packet_t *packet);
/** @brief Pop a packet from the local free-stack. */
static tx_packet_t *pop_free_tx_packet(void);
/** @brief Queue a transmitted packet to be reclaimed by the DSP side. */
static int queue_returned_tx_packet(tx_packet_t *packet);
/** @brief COBS-encode the compact normal-stream payload. */
static size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output, size_t output_size);

void tx_packet_service_reset(void)
{
	working_tx_packet = &tx_packet_buffer_a;
	free_tx_packet_stack[0] = &tx_packet_buffer_b;
	free_tx_packet_stack[1] = &tx_packet_buffer_c;
	free_tx_packet_count = 2U;
	(void)exchange_ready_tx_packet(NULL);
	store_returned_tx_write_index(0U);
	store_returned_tx_read_index(0U);
	for (uint32_t index = 0U; index < TX_RETURN_QUEUE_LEN; index++)
	{
		returned_tx_packet_queue[index] = NULL;
	}
	memset((void *)&tx_last_sent_debug, 0, sizeof(tx_last_sent_debug));
}

void tx_packet_service_initialize_ownership(void)
{
	tx_packet_service_reset();
}

tx_packet_t *tx_packet_service_get_working_packet(void)
{
	return working_tx_packet;
}

void tx_packet_service_publish_working_packet(void)
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

void tx_packet_service_reclaim_returned_packets(void)
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

void tx_packet_service_claim_latest_packet(tx_packet_t **current_tx_packet)
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

uint32_t tx_packet_service_get_sample_latency_us(const tx_packet_t *packet)
{
	uint32_t reference_timestamp_us = app_mode_service_get_button_event_timestamp_us();

	if (reference_timestamp_us >= packet->sample_ready_timestamp_us)
	{
		return reference_timestamp_us - packet->sample_ready_timestamp_us;
	}

	return tim2_get_timestamp_us() - packet->sample_ready_timestamp_us;
}

int tx_packet_service_serialize(const tx_packet_t *pkt, uint32_t sample_latency_us, char *buf, size_t size)
{
	tx_stream_payload_t payload;
	size_t encoded_length;

	if ((pkt == NULL) || (buf == NULL) || (size < (sizeof(payload) + 2U)))
	{
		return -1;
	}

	payload.age_us = sample_latency_us;
	payload.status_flags = pkt->status_flags;
	payload.filtered_vdda_mv = (uint16_t)pkt->filtered_vdda_mv[0];
	payload.filtered_temperature_c = (int16_t)pkt->filtered_temperature_c[0];
	for (uint32_t channel = 0U; channel < ADC_CH_PER_ADC; channel++)
	{
		payload.filtered_adc1_mv[channel] = (uint16_t)pkt->filtered_adc1_mv[0][channel];
		payload.filtered_adc2_mv[channel] = (uint16_t)pkt->filtered_adc2_mv[0][channel];
	}

	encoded_length = cobs_encode((const uint8_t *)&payload, sizeof(payload), (uint8_t *)buf, size);
	if ((encoded_length == 0U) || (encoded_length >= size))
	{
		return -1;
	}

	((uint8_t *)buf)[encoded_length] = 0U;
	return (int)(encoded_length + 1U);
}

void tx_packet_service_store_last_sent_debug(const tx_packet_t *packet)
{
	tx_last_sent_debug = *packet;
}

static size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output, size_t output_size)
{
	size_t read_index = 0U;
	size_t write_index = 1U;
	size_t code_index = 0U;
	uint8_t code = 1U;

	if ((input == NULL) || (output == NULL) || (output_size == 0U))
	{
		return 0U;
	}

	while (read_index < length)
	{
		if (write_index >= output_size)
		{
			return 0U;
		}

		if (input[read_index] == 0U)
		{
			output[code_index] = code;
			code = 1U;
			code_index = write_index;
			write_index++;
			read_index++;
		}
		else
		{
			output[write_index] = input[read_index];
			write_index++;
			read_index++;
			code++;

			if (code == 0xFFU)
			{
				output[code_index] = code;
				code = 1U;
				code_index = write_index;
				if (write_index >= output_size)
				{
					return 0U;
				}
				write_index++;
			}
		}
	}

	if (code_index >= output_size)
	{
		return 0U;
	}

	output[code_index] = code;
	return write_index;
}

static tx_packet_t *exchange_ready_tx_packet(tx_packet_t *packet)
{
	return __atomic_exchange_n(&ready_tx_packet, packet, __ATOMIC_ACQ_REL);
}

static uint32_t load_returned_tx_write_index(void)
{
	return __atomic_load_n(&returned_tx_packet_write_index, __ATOMIC_ACQUIRE);
}

static uint32_t load_returned_tx_read_index(void)
{
	return __atomic_load_n(&returned_tx_packet_read_index, __ATOMIC_RELAXED);
}

static void store_returned_tx_write_index(uint32_t write_index)
{
	__atomic_store_n(&returned_tx_packet_write_index, write_index, __ATOMIC_RELEASE);
}

static void store_returned_tx_read_index(uint32_t read_index)
{
	__atomic_store_n(&returned_tx_packet_read_index, read_index, __ATOMIC_RELEASE);
}

static void push_free_tx_packet(tx_packet_t *packet)
{
	if ((packet == NULL) || (free_tx_packet_count >= 2U))
	{
		Error_Handler();
	}

	free_tx_packet_stack[free_tx_packet_count] = packet;
	free_tx_packet_count++;
}

static tx_packet_t *pop_free_tx_packet(void)
{
	if (free_tx_packet_count == 0U)
	{
		return NULL;
	}

	free_tx_packet_count--;
	return free_tx_packet_stack[free_tx_packet_count];
}

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