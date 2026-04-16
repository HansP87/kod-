#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "app_runtime.h"
#include "monitor_task.h"
#include "tim.h"
#include "usart.h"

#define MONITOR_STARTUP_BANNER_PERIOD_MS  250U
#define MONITOR_STARTUP_BANNER_REPEAT_COUNT  8U
#define MONITOR_AUTO_TRIGGER_PERIOD_MS  2000U

extern osThreadId_t transmit_task_handle;

/** Send a raw monitor string over USART1. */
static void monitor_uart_send(const char *message);
/** Emit the boot synchronization banner that carries the MCU serial number. */
static void monitor_send_startup_banner(void);

/**
 * @brief Publish boot synchronization banners and periodic auto-trigger events.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_monitor_task(void *argument)
{
	uint32_t startup_banner_elapsed_ms = 0U;
	uint32_t startup_banner_count = 0U;
	uint32_t auto_trigger_elapsed_ms = 0U;

	(void)argument;

	monitor_send_startup_banner();
	startup_banner_count = 1U;

	for (;;)
	{
		if (app_runtime_is_streaming_mode() != 0U)
		{
			if (startup_banner_count < MONITOR_STARTUP_BANNER_REPEAT_COUNT)
			{
				if (startup_banner_elapsed_ms >= MONITOR_STARTUP_BANNER_PERIOD_MS)
				{
					monitor_send_startup_banner();
					startup_banner_elapsed_ms = 0U;
					startup_banner_count++;
				}
				else
				{
					startup_banner_elapsed_ms++;
				}
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
		}
		else
		{
			startup_banner_elapsed_ms = 0U;
			auto_trigger_elapsed_ms = 0U;
		}

		osDelay(1);
	}
}

/**
 * @brief Send a monitor message over the shared UART port.
 * @param message Null-terminated ASCII message to transmit.
 */
static void monitor_uart_send(const char *message)
{
	(void)HAL_UART_Transmit(&huart1, (const uint8_t *)message, (uint16_t)strlen(message), HAL_MAX_DELAY);
}

/**
 * @brief Emit the startup banner used by the host test harness for boot synchronization.
 */
static void monitor_send_startup_banner(void)
{
	char banner[48];

	(void)snprintf(
			banner,
			sizeof(banner),
			"STARTUP:MCU_SERIAL=%08lX%08lX%08lX\n",
			(unsigned long)HAL_GetUIDw0(),
			(unsigned long)HAL_GetUIDw1(),
			(unsigned long)HAL_GetUIDw2());
	monitor_uart_send(banner);
}
