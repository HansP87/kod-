#include <stdio.h>
#include <string.h>
#include "app_mode_service.h"
#include "app_task_flags.h"
#include "monitor_service.h"
#include "tim.h"
#include "transmit_service.h"
#include "usart.h"

#define MONITOR_STARTUP_BANNER_PERIOD_MS     250U
#define MONITOR_STARTUP_BANNER_REPEAT_COUNT  1U

static uint32_t startup_banner_elapsed_ms = 0U;
static uint32_t startup_banner_count = 0U;
static volatile uint32_t pending_button_trigger = 0U;

static void monitor_uart_send(const char *message);
static void monitor_send_startup_banner(void);

void monitor_service_initialize(void)
{
	startup_banner_elapsed_ms = 0U;
	startup_banner_count = 0U;
	pending_button_trigger = 0U;

	monitor_send_startup_banner();
	startup_banner_count = 1U;
}

void monitor_service_notify_button_press(void)
{
	__atomic_store_n(&pending_button_trigger, 1U, __ATOMIC_RELEASE);
}

void monitor_service_process_tick(osThreadId_t transmit_task_handle)
{
	(void)transmit_task_handle;

	if (app_mode_service_is_streaming_mode() != 0U)
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

		if ((__atomic_exchange_n(&pending_button_trigger, 0U, __ATOMIC_ACQ_REL) != 0U) &&
				(transmit_service_is_high_rate_capture_active() == 0U))
		{
			monitor_uart_send("TEST:AUTO_TRIGGER\n");
		}
	}
	else
	{
		startup_banner_elapsed_ms = 0U;
		pending_button_trigger = 0U;
	}
}

static void monitor_uart_send(const char *message)
{
	(void)usart1_transmit_blocking((const uint8_t *)message, (uint16_t)strlen(message), HAL_MAX_DELAY);
}

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