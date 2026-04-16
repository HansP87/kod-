#include "cmsis_os2.h"
#include "main.h"
#include "usart.h"
#include "app_runtime.h"
#include "transmit_task.h"

static char tx_message_buffer[512];

void start_transmit_task(void *argument)
{
	uint32_t flags;
	tx_packet_t *current_tx_packet = NULL;
	uint32_t sample_latency_us;
	int length;

	(void)argument;

	for (;;)
	{
		flags = osThreadFlagsWait(TX_FLAG_SEND, osFlagsWaitAny, osWaitForever);

		if ((flags & TX_FLAG_SEND) != 0U)
		{
			app_runtime_claim_latest_tx_packet(&current_tx_packet);

			if (current_tx_packet != NULL)
			{
				sample_latency_us = app_runtime_get_sample_latency_us(current_tx_packet);
				length = app_runtime_serialize_tx_packet(
						current_tx_packet,
						sample_latency_us,
						tx_message_buffer,
						sizeof(tx_message_buffer));
				if (length > 0)
				{
					HAL_UART_Transmit(&huart1, (uint8_t *)tx_message_buffer, (uint16_t)length, HAL_MAX_DELAY);
				}

				app_runtime_store_last_sent_debug(current_tx_packet);
			}
		}
	}
}
