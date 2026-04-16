#include "cmsis_os2.h"
#include "default_task.h"

/**
 * @brief Idle background thread reserved for future non-critical work.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_default_task(void *argument)
{
	(void)argument;

	for (;;)
	{
		osDelay(1000);
	}
}
