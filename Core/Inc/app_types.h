#ifndef APP_TYPES_H
#define APP_TYPES_H

/**
 * @file app_types.h
 * @brief Shared application-wide constants and packet types.
 * @ingroup app_core
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Number of rank-ordered channels sampled on each ADC instance. */
#define ADC_CH_PER_ADC          4U
/** Number of raw samples captured per DSP frame before decimation. */
#define ADC_SAMPLES_PER_DSP     4U
/** CIC decimation factor applied before publishing filtered data. */
#define CIC_DECIMATION_FACTOR   4U
/** Number of filtered samples published per DSP frame. */
#define FILTERED_SAMPLES_PER_DSP (ADC_SAMPLES_PER_DSP / CIC_DECIMATION_FACTOR)

/** Supported top-level application modes. */
typedef enum
{
	/** Normal streaming mode with packet publication enabled. */
	APP_MODE_STREAMING = 0,
	/** Configuration mode with streaming suspended and command responses enabled. */
	APP_MODE_CONFIG = 1,
} app_mode_t;

/**
 * @brief Full DSP packet containing raw and filtered ADC results for one scheduler frame.
 */
typedef struct
{
	/** Monotonic frame sequence number. */
	uint32_t sequence;
	/** Logical packet timestamp currently aligned with the sequence counter. */
	uint32_t timestamp;
	/** Timestamp in microseconds when the most recent ADC frame became available. */
	uint32_t sample_ready_timestamp_us;

	/** Raw VDDA estimate for each raw sample in the frame. */
	uint32_t vdda_mv[ADC_SAMPLES_PER_DSP];
	/** Raw MCU temperature estimate for each raw sample in the frame. */
	int32_t temperature_c[ADC_SAMPLES_PER_DSP];

	/** Raw ADC1 channel voltages in millivolts, indexed by sample then rank. */
	uint32_t adc1_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];
	/** Raw ADC2 channel voltages in millivolts, indexed by sample then rank. */
	uint32_t adc2_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];

	/** Filtered VDDA estimate after decimation. */
	uint32_t filtered_vdda_mv[FILTERED_SAMPLES_PER_DSP];
	/** Filtered MCU temperature estimate after decimation. */
	int32_t filtered_temperature_c[FILTERED_SAMPLES_PER_DSP];
	/** Filtered ADC1 channel voltages in millivolts, indexed by filtered sample then rank. */
	uint32_t filtered_adc1_mv[FILTERED_SAMPLES_PER_DSP][ADC_CH_PER_ADC];
	/** Filtered ADC2 channel voltages in millivolts, indexed by filtered sample then rank. */
	uint32_t filtered_adc2_mv[FILTERED_SAMPLES_PER_DSP][ADC_CH_PER_ADC];

	/** Bitmask of DSP/transport status flags associated with the frame. */
	uint32_t status_flags;
} tx_packet_t;

#ifdef __cplusplus
}
#endif

#endif