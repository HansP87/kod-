#ifndef SAMPLING_PIPELINE_SERVICE_H
#define SAMPLING_PIPELINE_SERVICE_H

/**
 * @file sampling_pipeline_service.h
 * @brief ADC acquisition, decimation, and filtered packet preparation pipeline.
 * @ingroup sampling_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

/** @brief Reset sampling-pipeline state, filters, counters, and timestamps. */
void sampling_pipeline_service_reset(void);

/** @brief Start the ADC DMA engines and the timer-driven sampling pipeline. */
void sampling_pipeline_service_start(void);

/**
 * @brief Process one DSP scheduler tick and publish a packet when fresh ADC data is ready.
 * @param last_adc1_frame Caller-owned last-seen ADC1 frame counter.
 * @param last_adc2_frame Caller-owned last-seen ADC2 frame counter.
 */
void sampling_pipeline_service_process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame);

/**
 * @brief Record that one ADC DMA frame has completed.
 * @param hadc ADC instance that completed the frame.
 */
void sampling_pipeline_service_record_adc_frame_ready(ADC_HandleTypeDef *hadc);

#ifdef __cplusplus
}
#endif

#endif