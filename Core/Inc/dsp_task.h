#ifndef DSP_TASK_H
#define DSP_TASK_H

/**
 * @file dsp_task.h
 * @brief Entry point for the DSP scheduler task.
 * @ingroup task_entrypoints
 */

/** @brief Run the DSP pipeline task that prepares outgoing ADC packets. */
void start_dsp_task(void *argument);

#endif
