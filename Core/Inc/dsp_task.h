#ifndef DSP_TASK_H
#define DSP_TASK_H

/**
 * @brief Run the DSP pipeline task that prepares outgoing ADC packets.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_dsp_task(void *argument);

#endif
