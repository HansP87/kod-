#include "cmsis_os2.h"
#include "monitor_service.h"
#include "monitor_task.h"

extern osThreadId_t transmit_task_handle;

/**
 * @brief Publish boot synchronization banners and periodic auto-trigger events.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_monitor_task(void *argument)
{
	(void)argument;
	monitor_service_initialize();

	for (;;)
	{
		monitor_service_process_tick(transmit_task_handle);
		osDelay(1);
	}
}
