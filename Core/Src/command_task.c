/**
 * @file command_task.c
 * @brief UART RX state machine for ASCII configuration commands and binary capture requests.
 * @ingroup command_services
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_mode_service.h"
#include "cmsis_os2.h"
#include "command_protocol.h"
#include "command_service.h"
#include "main.h"
#include "command_task.h"
#include "transmit_service.h"
#include "usart.h"

#define COMMAND_RX_BUFFER_LEN           128U
#define COMMAND_BINARY_BUFFER_LEN       32U
#define COMMAND_RX_QUEUE_LEN            128U
#define COMMAND_RESPONSE_BUFFER_LEN     160U

#define HIGH_RATE_REQUEST_TYPE_START    1U
#define HIGH_RATE_REQUEST_PROTOCOL_VERSION 1U

/** Send a raw command response over USART1. */
static void command_uart_send(const char *message);
/** Arm interrupt-driven reception of the next UART byte. */
static void command_uart_rx_start(void);
/** Pop one queued UART byte if available. */
static uint32_t command_uart_dequeue_byte(uint8_t *byte);
/** Push one received UART byte into the task-owned queue. */
static void command_uart_enqueue_byte(uint8_t byte);
/** Handle a complete command line collected from the UART stream. */
static void process_received_line(const char *line);
/** Handle one zero-delimited binary command frame. */
static void process_received_binary_frame(const uint8_t *frame, size_t length);
/** Build and send a framed transport-level error response. */
static void send_transport_error_response(const char *error_code);
/** Decode one COBS payload into a destination buffer. */
static size_t cobs_decode(const uint8_t *input, size_t length, uint8_t *output, size_t output_size);

extern osThreadId_t command_task_handle;

static uint8_t command_rx_queue[COMMAND_RX_QUEUE_LEN];
static volatile uint32_t command_rx_queue_head = 0U;
static volatile uint32_t command_rx_queue_tail = 0U;
static uint8_t command_rx_byte = 0U;

typedef struct __attribute__((packed))
{
	uint8_t request_type;
	uint8_t protocol_version;
	uint32_t frame_count;
} high_rate_stream_request_t;

/**
 * @brief Run the command parser and configuration-mode state machine.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_command_task(void *argument)
{
	char rx_buffer[COMMAND_RX_BUFFER_LEN];
	uint8_t binary_buffer[COMMAND_BINARY_BUFFER_LEN];
	size_t rx_length = 0U;
	size_t binary_length = 0U;
	uint8_t received_byte;
	uint32_t binary_active = 0U;

	(void)argument;
	command_uart_rx_start();

	for (;;)
	{
		if (command_uart_dequeue_byte(&received_byte) != 0U)
		{
			if (binary_active != 0U)
			{
				if (received_byte == 0U)
				{
					if (binary_length > 0U)
					{
						process_received_binary_frame(binary_buffer, binary_length);
					}
					binary_active = 0U;
					binary_length = 0U;
					rx_length = 0U;
				}
				else if (binary_length < sizeof(binary_buffer))
				{
					binary_buffer[binary_length] = received_byte;
					binary_length++;
				}
				else
				{
					binary_active = 0U;
					binary_length = 0U;
				}
			}
			else if (received_byte == 0U)
			{
				binary_active = 1U;
				binary_length = 0U;
				rx_length = 0U;
			}
			else if ((received_byte == '\r') || (received_byte == '\n'))
			{
				if (rx_length > 0U)
				{
					rx_buffer[rx_length] = '\0';
					process_received_line(rx_buffer);
					rx_length = 0U;
				}
			}
			else if (rx_length + 1U < sizeof(rx_buffer))
			{
				rx_buffer[rx_length] = (char)received_byte;
				rx_length++;
			}
			else
			{
				rx_length = 0U;
				if (app_mode_service_get_mode() == APP_MODE_CONFIG)
				{
					send_transport_error_response("TOO_LONG");
				}
			}
		}
		else
		{
			(void)osThreadFlagsWait(COMMAND_TASK_FLAG_RX, osFlagsWaitAny, 5U);
		}
	}
}

/**
 * @brief Send a raw ASCII response over USART1.
 * @param message Null-terminated ASCII message to transmit.
 */
static void command_uart_send(const char *message)
{
	(void)usart1_transmit_blocking((const uint8_t *)message, (uint16_t)strlen(message), HAL_MAX_DELAY);
}

/**
 * @brief Reset the RX queue state and arm the first interrupt-driven receive.
 */
static void command_uart_rx_start(void)
{
	command_rx_queue_head = 0U;
	command_rx_queue_tail = 0U;
	(void)HAL_UART_Receive_IT(&huart1, &command_rx_byte, 1U);
}

/**
 * @brief Remove one byte from the queued UART RX data.
 * @param byte Destination for the dequeued byte.
 * @return Non-zero when a byte was dequeued, otherwise zero.
 */
static uint32_t command_uart_dequeue_byte(uint8_t *byte)
{
	uint32_t tail_index;

	if (command_rx_queue_tail == command_rx_queue_head)
	{
		return 0U;
	}

	tail_index = command_rx_queue_tail;
	*byte = command_rx_queue[tail_index];
	command_rx_queue_tail = (tail_index + 1U) % COMMAND_RX_QUEUE_LEN;
	return 1U;
}

/**
 * @brief Queue one received UART byte for the command task.
 * @param byte Newly received UART byte.
 */
static void command_uart_enqueue_byte(uint8_t byte)
{
	uint32_t next_head = (command_rx_queue_head + 1U) % COMMAND_RX_QUEUE_LEN;

	if (next_head != command_rx_queue_tail)
	{
		command_rx_queue[command_rx_queue_head] = byte;
		command_rx_queue_head = next_head;
	}
}

/**
 * @brief Process one completed ASCII line from the command UART.
 * @param line Null-terminated command line without CR/LF terminators.
 */
static void process_received_line(const char *line)
{
	char response[COMMAND_RESPONSE_BUFFER_LEN];
	command_service_action_t action;
	uint32_t frame_count;
	unsigned long requested_frame_count;

	if (sscanf(line, "TEST:HR_STREAM %lu", &requested_frame_count) == 1)
	{
		frame_count = (uint32_t)requested_frame_count;
		if (transmit_service_start_high_rate_capture(frame_count) == 0U)
		{
			command_uart_send("TEST:HR_STREAM_BUSY\n");
		}
		return;
	}

	if (command_service_process_line(line, response, sizeof(response), &action) != 0U)
	{
		command_uart_send(response);

		if (action == COMMAND_SERVICE_ACTION_REBOOT)
		{
			osDelay(20U);
			__DSB();
			NVIC_SystemReset();
		}
	}
}

static void process_received_binary_frame(const uint8_t *frame, size_t length)
{
	uint8_t decoded_frame[COMMAND_BINARY_BUFFER_LEN];
	size_t decoded_length;
	high_rate_stream_request_t request;

	decoded_length = cobs_decode(frame, length, decoded_frame, sizeof(decoded_frame));
	if (decoded_length != sizeof(request))
	{
		return;
	}

	(void)memcpy(&request, decoded_frame, sizeof(request));
	if ((request.request_type != HIGH_RATE_REQUEST_TYPE_START) ||
			(request.protocol_version != HIGH_RATE_REQUEST_PROTOCOL_VERSION))
	{
		return;
	}

	(void)transmit_service_start_high_rate_capture(request.frame_count);
}

/**
 * @brief Send a framed transport-level error response.
 * @param error_code Protocol error code to place in the response payload.
 */
static void send_transport_error_response(const char *error_code)
{
	char response[COMMAND_RESPONSE_BUFFER_LEN];

	if (command_protocol_build_response("ERROR", error_code, response, sizeof(response)) > 0)
	{
		command_uart_send(response);
	}
}

static size_t cobs_decode(const uint8_t *input, size_t length, uint8_t *output, size_t output_size)
{
	size_t read_index = 0U;
	size_t write_index = 0U;

	if ((input == NULL) || (output == NULL))
	{
		return 0U;
	}

	while (read_index < length)
	{
		uint8_t code = input[read_index];
		size_t copy_length;

		if (code == 0U)
		{
			return 0U;
		}

		read_index++;
		copy_length = (size_t)code - 1U;
		if ((read_index + copy_length > length) || (write_index + copy_length > output_size))
		{
			return 0U;
		}

		(void)memcpy(&output[write_index], &input[read_index], copy_length);
		write_index += copy_length;
		read_index += copy_length;

		if ((code != 0xFFU) && (read_index < length))
		{
			if (write_index >= output_size)
			{
				return 0U;
			}
			output[write_index] = 0U;
			write_index++;
		}
	}

	return write_index;
}

/**
 * @brief Queue one received USART1 byte and wake the command task.
 * @param huart UART instance that completed the interrupt-driven receive.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		command_uart_enqueue_byte(command_rx_byte);
		(void)HAL_UART_Receive_IT(&huart1, &command_rx_byte, 1U);
		(void)osThreadFlagsSet(command_task_handle, COMMAND_TASK_FLAG_RX);
	}
}

/**
 * @brief Recover the interrupt-driven UART receive path after a USART1 error.
 * @param huart UART instance that reported the error.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		__HAL_UART_CLEAR_OREFLAG(huart);
		__HAL_UART_CLEAR_NEFLAG(huart);
		__HAL_UART_CLEAR_FEFLAG(huart);
		(void)HAL_UART_Receive_IT(&huart1, &command_rx_byte, 1U);
	}
}