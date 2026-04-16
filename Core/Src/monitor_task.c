#include <string.h>
#include "cmsis_os2.h"
#include "main.h"
#include "app_runtime.h"
#include "monitor_task.h"
#include "tim.h"
#include "usart.h"

#define MONITOR_COMMAND_BUFFER_LEN  32U
#define MONITOR_READY_PERIOD_MS  1000U
#define MONITOR_AUTO_TRIGGER_PERIOD_MS  2000U

extern osThreadId_t transmit_task_handle;

static void monitor_uart_send(const char *message);
static void handle_monitor_command(const char *command);

void start_monitor_task(void *argument)
{
	uint8_t received_byte;
	char command_buffer[MONITOR_COMMAND_BUFFER_LEN];
	size_t command_length = 0U;
	HAL_StatusTypeDef uart_status;
	uint32_t ready_elapsed_ms = 0U;
	uint32_t auto_trigger_elapsed_ms = 0U;

	(void)argument;

	monitor_uart_send("TEST:READY\n");

	for (;;)
	{
		uart_status = HAL_UART_Receive(&huart1, &received_byte, 1U, 0U);

		if (uart_status == HAL_OK)
		{
			if ((received_byte == '\r') || (received_byte == '\n'))
			{
				if (command_length > 0U)
				{
					command_buffer[command_length] = '\0';
					handle_monitor_command(command_buffer);
					command_length = 0U;
				}
			}
			else if (command_length + 1U < sizeof(command_buffer))
			{
				if ((received_byte >= (uint8_t)'a') && (received_byte <= (uint8_t)'z'))
				{
					received_byte = (uint8_t)(received_byte - ((uint8_t)'a' - (uint8_t)'A'));
				}

				command_buffer[command_length] = (char)received_byte;
				command_length++;
			}
			else
			{
				command_length = 0U;
				monitor_uart_send("ERR:COMMAND_TOO_LONG\n");
			}
		}

		if (ready_elapsed_ms >= MONITOR_READY_PERIOD_MS)
		{
			monitor_uart_send("TEST:READY\n");
			ready_elapsed_ms = 0U;
		}
		else
		{
			ready_elapsed_ms++;
		}

		if (auto_trigger_elapsed_ms >= MONITOR_AUTO_TRIGGER_PERIOD_MS)
		{
			monitor_uart_send("TEST:AUTO_TRIGGER\n");
			app_runtime_set_button_event_timestamp_us(tim2_get_timestamp_us());
			osThreadFlagsSet(transmit_task_handle, TX_FLAG_SEND);
			auto_trigger_elapsed_ms = 0U;
		}
		else
		{
			auto_trigger_elapsed_ms++;
		}

		osDelay(1);
	}
}

static void monitor_uart_send(const char *message)
{
	(void)HAL_UART_Transmit(&huart1, (const uint8_t *)message, (uint16_t)strlen(message), HAL_MAX_DELAY);
}

static void handle_monitor_command(const char *command)
{
	if (strcmp(command, "PING") == 0)
	{
		monitor_uart_send("PONG\n");
		return;
	}

	if (strcmp(command, "HELP") == 0)
	{
		monitor_uart_send("OK:COMMANDS PING HELP TRIGGER\n");
		return;
	}

	if (strcmp(command, "TRIGGER") == 0)
	{
		monitor_uart_send("OK:TRIGGER\n");
		app_runtime_set_button_event_timestamp_us(tim2_get_timestamp_us());
		osThreadFlagsSet(transmit_task_handle, TX_FLAG_SEND);
		return;
	}

	monitor_uart_send("ERR:UNKNOWN_COMMAND\n");
}
