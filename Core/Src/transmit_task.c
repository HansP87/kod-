#include "cmsis_os2.h"
#include "app_task_flags.h"
#include "transmit_service.h"
#include "transmit_task.h"

/**
 * @brief Wait for transmit requests and forward the latest packet over USART1.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_transmit_task(void *argument)
{
	uint32_t flags;

	(void)argument;
	transmit_service_reset();

	for (;;)
	{
		flags = osThreadFlagsWait(TX_FLAG_SEND, osFlagsWaitAny, osWaitForever);

		if ((flags & TX_FLAG_SEND) != 0U)
		{
			transmit_service_process_send_request();
		}
	}
}
