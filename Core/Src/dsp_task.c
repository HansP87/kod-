#include "cmsis_os2.h"
#include "app_runtime.h"
#include "dsp_task.h"

void start_dsp_task(void *argument)
{
	uint32_t flags;
	uint32_t last_adc1_frame = 0U;
	uint32_t last_adc2_frame = 0U;

	(void)argument;

	app_runtime_reset();
	app_runtime_initialize_tx_buffer_ownership();
	app_runtime_start_sampling_pipeline();

	for (;;)
	{
		flags = osThreadFlagsWait(DSP_FLAG_TICK, osFlagsWaitAny, osWaitForever);

		if ((flags & DSP_FLAG_TICK) != 0U)
		{
			app_runtime_process_dsp_tick(&last_adc1_frame, &last_adc2_frame);
		}
	}
}
