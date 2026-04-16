#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmsis_os2.h"
#include "main.h"
#include "app_runtime.h"
#include "command_task.h"
#include "usart.h"

#define COMMAND_RX_BUFFER_LEN           128U
#define COMMAND_RX_QUEUE_LEN            128U
#define COMMAND_MAX_TOKENS              8U
#define COMMAND_RESPONSE_BUFFER_LEN     160U
#define COMMAND_FRAME_BUFFER_LEN        (COMMAND_RESPONSE_BUFFER_LEN + 8U)
#define COMMAND_REQUEST_SIGN            '@'
#define COMMAND_RESPONSE_SIGN           '!'
#define COMMAND_CRC8_POLYNOMIAL         0x07U
#define COMMAND_CRC8_SEED               0xA5U

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
/** Decode and execute a framed configuration command. */
static void process_config_frame(const char *frame);
/** Build and send a framed error response. */
static void send_error_response(const char *error_code);
/** Send the MCU unique ID through the framed command protocol. */
static void send_serial_response(void);
/** Leave configuration mode and resume streaming mode. */
static void send_exit_config_response(void);
/** Acknowledge the reset command and reboot the MCU. */
static void send_reset_response_and_reboot(void);
/** Compute the protocol CRC-8 over an ASCII payload. */
static uint8_t compute_crc8(const uint8_t *data, size_t length);
/** Parse a two-digit hexadecimal CRC field. */
static int parse_crc8_field(const char *text, uint8_t *value);
/** Build a framed protocol response with an optional parameter field. */
static void build_and_send_response(const char *command_name, const char *parameter0);

extern osThreadId_t command_task_handle;

static uint8_t command_rx_queue[COMMAND_RX_QUEUE_LEN];
static volatile uint32_t command_rx_queue_head = 0U;
static volatile uint32_t command_rx_queue_tail = 0U;
static uint8_t command_rx_byte = 0U;

/**
 * @brief Run the command parser and configuration-mode state machine.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_command_task(void *argument)
{
	char rx_buffer[COMMAND_RX_BUFFER_LEN];
	size_t rx_length = 0U;
	uint8_t received_byte;

	(void)argument;
	command_uart_rx_start();

	for (;;)
	{
		if (command_uart_dequeue_byte(&received_byte) != 0U)
		{
			if ((received_byte == '\r') || (received_byte == '\n'))
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
				if (app_runtime_get_mode() == APP_MODE_CONFIG)
				{
					send_error_response("TOO_LONG");
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
	(void)HAL_UART_Transmit(&huart1, (const uint8_t *)message, (uint16_t)strlen(message), HAL_MAX_DELAY);
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
	if (strcmp(line, "CONFIGMODE") == 0)
	{
		app_runtime_set_mode(APP_MODE_CONFIG);
		command_uart_send("ENTERED CONFIGMODE\n");
		return;
	}

	if (app_runtime_get_mode() == APP_MODE_CONFIG)
	{
		process_config_frame(line);
	}
}

/**
 * @brief Parse and dispatch a framed configuration command.
 * @param frame Null-terminated framed command line.
 */
static void process_config_frame(const char *frame)
{
	char mutable_frame[COMMAND_RX_BUFFER_LEN];
	char *tokens[COMMAND_MAX_TOKENS];
	char *cursor;
	char *last_comma;
	uint8_t received_crc = 0U;
	uint8_t expected_crc;
	uint32_t token_count = 0U;

	if (strlen(frame) >= sizeof(mutable_frame))
	{
		send_error_response("TOO_LONG");
		return;
	}

	last_comma = strrchr(frame, ',');
	if (last_comma == NULL)
	{
		send_error_response("MALFORMED");
		return;
	}

	if (parse_crc8_field(last_comma + 1, &received_crc) == 0)
	{
		send_error_response("BADCRC");
		return;
	}

	expected_crc = compute_crc8((const uint8_t *)frame, (size_t)(last_comma - frame));
	if (expected_crc != received_crc)
	{
		send_error_response("BADCRC");
		return;
	}

	(void)strcpy(mutable_frame, frame);
	cursor = strtok(mutable_frame, ",");
	while ((cursor != NULL) && (token_count < COMMAND_MAX_TOKENS))
	{
		tokens[token_count] = cursor;
		token_count++;
		cursor = strtok(NULL, ",");
	}

	if (token_count < 3U)
	{
		send_error_response("MALFORMED");
		return;
	}

	if ((strlen(tokens[0]) != 1U) || (tokens[0][0] != COMMAND_REQUEST_SIGN))
	{
		send_error_response("BADSIGN");
		return;
	}

	if (strcmp(tokens[1], "mcu_serial") == 0)
	{
		send_serial_response();
		return;
	}

	if (strcmp(tokens[1], "exit_config") == 0)
	{
		send_exit_config_response();
		return;
	}

	if (strcmp(tokens[1], "reset") == 0)
	{
		send_reset_response_and_reboot();
		return;
	}

	send_error_response("UNKNOWN_COMMAND");
}

/**
 * @brief Send a framed error response.
 * @param error_code Symbolic error string to place in the response payload.
 */
static void send_error_response(const char *error_code)
{
	build_and_send_response("ERROR", error_code);
}

/**
 * @brief Send the MCU unique ID as a framed command response.
 */
static void send_serial_response(void)
{
	char serial_text[25];
	(void)snprintf(
			serial_text,
			sizeof(serial_text),
			"%08lX%08lX%08lX",
			(unsigned long)HAL_GetUIDw0(),
			(unsigned long)HAL_GetUIDw1(),
			(unsigned long)HAL_GetUIDw2());
	build_and_send_response("MCU_SERIAL", serial_text);
}

/**
 * @brief Acknowledge config-mode exit and resume streaming mode.
 */
static void send_exit_config_response(void)
{
	build_and_send_response("EXIT_CONFIG", "STREAMING");
	app_runtime_set_mode(APP_MODE_STREAMING);
}

/**
 * @brief Acknowledge the reset command and trigger a system reset.
 */
static void send_reset_response_and_reboot(void)
{
	build_and_send_response("RESET", "REBOOTING");
	osDelay(20U);
	__DSB();
	NVIC_SystemReset();
}

/**
 * @brief Compute the CRC-8 used by the framed UART command protocol.
 * @param data Payload bytes to hash.
 * @param length Number of bytes in @p data.
 * @return Calculated CRC-8 value.
 */
static uint8_t compute_crc8(const uint8_t *data, size_t length)
{
	uint8_t crc = COMMAND_CRC8_SEED;

	for (size_t index = 0U; index < length; index++)
	{
		crc ^= data[index];
		for (uint32_t bit = 0U; bit < 8U; bit++)
		{
			if ((crc & 0x80U) != 0U)
			{
				crc = (uint8_t)((crc << 1U) ^ COMMAND_CRC8_POLYNOMIAL);
			}
			else
			{
				crc <<= 1U;
			}
		}
	}

	return crc;
}

/**
 * @brief Parse a hexadecimal CRC field from ASCII.
 * @param text Null-terminated ASCII CRC field.
 * @param value Destination for the parsed byte.
 * @return Non-zero on success, otherwise zero.
 */
static int parse_crc8_field(const char *text, uint8_t *value)
{
	char *parse_end;
	unsigned long parsed_value;

	if ((text == NULL) || (strlen(text) == 0U) || (strlen(text) > 2U))
	{
		return 0;
	}

	for (size_t index = 0U; text[index] != '\0'; index++)
	{
		if (isxdigit((unsigned char)text[index]) == 0)
		{
			return 0;
		}
	}

	parsed_value = strtoul(text, &parse_end, 16);
	if ((*parse_end != '\0') || (parsed_value > 0xFFU))
	{
		return 0;
	}

	*value = (uint8_t)parsed_value;
	return 1;
}

/**
 * @brief Build and send a framed response including its CRC field.
 * @param command_name Response command token written after the response sign.
 * @param parameter0 Optional single parameter payload, or `NULL` for no parameter.
 */
static void build_and_send_response(const char *command_name, const char *parameter0)
{
	char payload[COMMAND_RESPONSE_BUFFER_LEN];
	char frame[COMMAND_FRAME_BUFFER_LEN];
	uint8_t crc;

	if (parameter0 != NULL)
	{
		(void)snprintf(payload, sizeof(payload), "%c,%s,%s", COMMAND_RESPONSE_SIGN, command_name, parameter0);
	}
	else
	{
		(void)snprintf(payload, sizeof(payload), "%c,%s", COMMAND_RESPONSE_SIGN, command_name);
	}

	crc = compute_crc8((const uint8_t *)payload, strlen(payload));
	(void)snprintf(frame, sizeof(frame), "%s,%02X\n", payload, (unsigned int)crc);
	command_uart_send(frame);
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