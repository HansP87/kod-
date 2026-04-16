#include "cmsis_os2.h"
#include "default_task.h"

void start_default_task(void *argument)
{
	(void)argument;

	for (;;)
	{
		osDelay(1000);
	}
}
