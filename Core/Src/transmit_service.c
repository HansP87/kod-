/**
 * @file transmit_service.c
 * @brief Normal-stream and high-rate capture serialization over USART1.
 * @ingroup transport_services
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "app_mode_service.h"
#include "app_task_flags.h"
#include "cmsis_os2.h"
#include "main.h"
#include "tim.h"
#include "tx_packet_service.h"
#include "transmit_service.h"
#include "usart.h"

#define HIGH_RATE_CAPTURE_PROTOCOL_VERSION 1U
#define HIGH_RATE_CAPTURE_IDLE       0U
#define HIGH_RATE_CAPTURE_STREAMING  1U

#define HIGH_RATE_FRAME_TYPE_BEGIN   1U
#define HIGH_RATE_FRAME_TYPE_END     3U

typedef struct __attribute__((packed))
{
	uint32_t age_us;
	uint32_t status_flags;
	uint16_t adc1_mv[ADC_CH_PER_ADC];
	uint16_t adc2_mv[ADC_CH_PER_ADC];
} high_rate_capture_payload_t;

typedef struct __attribute__((packed))
{
	uint8_t frame_type;
	uint8_t protocol_version;
	uint32_t frame_count;
} high_rate_capture_control_payload_t;

static uint8_t tx_message_buffer[1024];
static tx_packet_t *current_tx_packet = NULL;
static volatile uint32_t high_rate_capture_state = HIGH_RATE_CAPTURE_IDLE;
static volatile uint32_t high_rate_capture_total_frames = 0U;
static volatile uint32_t high_rate_capture_sent_frames = 0U;

extern osThreadId_t transmit_task_handle;

/** @brief Serialize one filtered packet into the high-rate binary payload format. */
static int serialize_high_rate_capture_frame(const tx_packet_t *packet, uint8_t *buf, size_t size);
/** @brief Serialize a high-rate begin/end control frame. */
static int serialize_high_rate_control_frame(uint8_t frame_type, uint32_t frame_count, uint8_t *buf, size_t size);
/** @brief Encode a payload with COBS for zero-delimited UART transport. */
static size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output, size_t output_size);

void transmit_service_reset(void)
{
	current_tx_packet = NULL;
	__atomic_store_n(&high_rate_capture_state, HIGH_RATE_CAPTURE_IDLE, __ATOMIC_RELEASE);
	__atomic_store_n(&high_rate_capture_total_frames, 0U, __ATOMIC_RELEASE);
	__atomic_store_n(&high_rate_capture_sent_frames, 0U, __ATOMIC_RELEASE);
}

void transmit_service_request_send(void)
{
	(void)osThreadFlagsSet(transmit_task_handle, TX_FLAG_SEND);
}

void transmit_service_process_send_request(void)
{
	uint32_t capture_state;
	uint32_t sent_frames;
	int length;

	if (app_mode_service_is_streaming_mode() == 0U)
	{
		return;
	}

	capture_state = __atomic_load_n(&high_rate_capture_state, __ATOMIC_ACQUIRE);
	if (capture_state == HIGH_RATE_CAPTURE_STREAMING)
	{
		tx_packet_service_claim_latest_packet(&current_tx_packet);
		if (current_tx_packet == NULL)
		{
			return;
		}

		length = serialize_high_rate_capture_frame(
				current_tx_packet,
				tx_message_buffer,
				sizeof(tx_message_buffer));
		if (length > 0)
		{
			(void)usart1_transmit_blocking((const uint8_t *)tx_message_buffer, (uint16_t)length, HAL_MAX_DELAY);
		}

		tx_packet_service_store_last_sent_debug(current_tx_packet);
		sent_frames = __atomic_load_n(&high_rate_capture_sent_frames, __ATOMIC_ACQUIRE) + 1U;
		__atomic_store_n(&high_rate_capture_sent_frames, sent_frames, __ATOMIC_RELEASE);
		if (sent_frames >= __atomic_load_n(&high_rate_capture_total_frames, __ATOMIC_ACQUIRE))
		{
			length = serialize_high_rate_control_frame(
					HIGH_RATE_FRAME_TYPE_END,
					sent_frames,
					tx_message_buffer,
					sizeof(tx_message_buffer));
			if (length > 0)
			{
				(void)usart1_transmit_blocking((const uint8_t *)tx_message_buffer, (uint16_t)length, HAL_MAX_DELAY);
			}
			__atomic_store_n(&high_rate_capture_state, HIGH_RATE_CAPTURE_IDLE, __ATOMIC_RELEASE);
		}
		return;
	}

	tx_packet_service_claim_latest_packet(&current_tx_packet);

	if (current_tx_packet == NULL)
	{
		return;
	}

	length = tx_packet_service_serialize(
			current_tx_packet,
			tx_packet_service_get_sample_latency_us(current_tx_packet),
				(char *)tx_message_buffer,
			sizeof(tx_message_buffer));
	if (length > 0)
	{
		(void)usart1_transmit_blocking((const uint8_t *)tx_message_buffer, (uint16_t)length, HAL_MAX_DELAY);
	}

	tx_packet_service_store_last_sent_debug(current_tx_packet);
}

uint32_t transmit_service_start_high_rate_capture(uint32_t frame_count)
{
	int length;

	if (frame_count == 0U)
	{
		return 0U;
	}

	if (__atomic_load_n(&high_rate_capture_state, __ATOMIC_ACQUIRE) != HIGH_RATE_CAPTURE_IDLE)
	{
		return 0U;
	}

	__atomic_store_n(&high_rate_capture_total_frames, frame_count, __ATOMIC_RELEASE);
	__atomic_store_n(&high_rate_capture_sent_frames, 0U, __ATOMIC_RELEASE);
	__atomic_store_n(&high_rate_capture_state, HIGH_RATE_CAPTURE_STREAMING, __ATOMIC_RELEASE);
	tx_message_buffer[0] = 0U;
	(void)usart1_transmit_blocking((const uint8_t *)tx_message_buffer, 1U, HAL_MAX_DELAY);
	length = serialize_high_rate_control_frame(
			HIGH_RATE_FRAME_TYPE_BEGIN,
			frame_count,
			tx_message_buffer,
			sizeof(tx_message_buffer));
	if (length > 0)
	{
		(void)usart1_transmit_blocking((const uint8_t *)tx_message_buffer, (uint16_t)length, HAL_MAX_DELAY);
	}

	return 1U;
}

uint32_t transmit_service_is_high_rate_capture_active(void)
{
	return __atomic_load_n(&high_rate_capture_state, __ATOMIC_ACQUIRE) != HIGH_RATE_CAPTURE_IDLE;
}

void transmit_service_capture_high_rate_frame(const tx_packet_t *packet)
{
	(void)packet;

	if (__atomic_load_n(&high_rate_capture_state, __ATOMIC_ACQUIRE) != HIGH_RATE_CAPTURE_STREAMING)
	{
		return;
	}

	if (__atomic_load_n(&high_rate_capture_sent_frames, __ATOMIC_ACQUIRE) >=
			__atomic_load_n(&high_rate_capture_total_frames, __ATOMIC_ACQUIRE))
	{
		return;
	}

	(void)osThreadFlagsSet(transmit_task_handle, TX_FLAG_SEND);
}

static int serialize_high_rate_capture_frame(const tx_packet_t *packet, uint8_t *buf, size_t size)
{
	high_rate_capture_payload_t payload;
	size_t encoded_length;

	payload.age_us = tim2_get_timestamp_us() - packet->sample_ready_timestamp_us;
	payload.status_flags = packet->status_flags;
	for (uint32_t channel = 0U; channel < ADC_CH_PER_ADC; channel++)
	{
		payload.adc1_mv[channel] = (uint16_t)packet->filtered_adc1_mv[0][channel];
		payload.adc2_mv[channel] = (uint16_t)packet->filtered_adc2_mv[0][channel];
	}

	encoded_length = cobs_encode((const uint8_t *)&payload, sizeof(payload), buf, size);
	if ((encoded_length == 0U) || (encoded_length >= size))
	{
		return -1;
	}

	buf[encoded_length] = 0U;
	return (int)(encoded_length + 1U);
}

static int serialize_high_rate_control_frame(uint8_t frame_type, uint32_t frame_count, uint8_t *buf, size_t size)
{
	high_rate_capture_control_payload_t payload;
	size_t encoded_length;

	payload.frame_type = frame_type;
	payload.protocol_version = HIGH_RATE_CAPTURE_PROTOCOL_VERSION;
	payload.frame_count = frame_count;

	encoded_length = cobs_encode((const uint8_t *)&payload, sizeof(payload), buf, size);
	if ((encoded_length == 0U) || (encoded_length >= size))
	{
		return -1;
	}

	buf[encoded_length] = 0U;
	return (int)(encoded_length + 1U);
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