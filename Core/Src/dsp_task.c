#include "cmsis_os2.h"
#include "app_mode_service.h"
#include "app_task_flags.h"
#include "sampling_pipeline_service.h"
#include "tx_packet_service.h"
#include "dsp_task.h"

/**
 * @brief Initialize the sampling pipeline and process each scheduler tick.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_dsp_task(void *argument)
{
	uint32_t flags;
	uint32_t last_adc1_frame = 0U;
	uint32_t last_adc2_frame = 0U;

	(void)argument;

	sampling_pipeline_service_reset();
	app_mode_service_set_mode(APP_MODE_STREAMING);
	tx_packet_service_initialize_ownership();
	app_mode_service_set_button_event_timestamp_us(0U);
	sampling_pipeline_service_start();

	for (;;)
	{
		flags = osThreadFlagsWait(DSP_FLAG_TICK, osFlagsWaitAny, osWaitForever);

		if ((flags & DSP_FLAG_TICK) != 0U)
		{
			sampling_pipeline_service_process_dsp_tick(&last_adc1_frame, &last_adc2_frame);
		}
	}
}
