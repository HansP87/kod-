/**
 * @file sampling_pipeline_service.c
 * @brief ADC DMA ingestion, CIC decimation, notch filtering, and packet preparation.
 * @ingroup sampling_services
 */

#include "cmsis_os2.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include "adc.h"
#include "lptim.h"
#include "stm32h7xx_hal_adc.h"
#include "tim.h"
#include "transmit_service.h"
#include "tx_packet_service.h"
#include "sampling_pipeline_service.h"

#define ADC_BUF_LEN             (ADC_CH_PER_ADC * ADC_SAMPLES_PER_DSP)
#define ADC2_TEMP_INDEX         2U
#define ADC2_VREF_INDEX         3U

#define ADC_TRIGGER_PERIOD      7499U
#define ADC_TRIGGER_PULSE       3750U
#define DSP_TICK_PERIOD         29999U
#define DSP_STARTUP_DELAY_MS    1U

#define NORMAL_STREAM_PERIOD_US 100000U

#define CIC_STAGE_COUNT         2U
#define CIC_GAIN_SHIFT          (2U * CIC_STAGE_COUNT)

#define DSP_OUTPUT_SAMPLE_RATE_HZ 800.0f
#define NOTCH_FILTER_Q          10.0f
#define NOTCH_FILTER_COUNT      3U
#define NOTCH_PI                3.14159265358979323846f

#define DSP_STATUS_OK           0x00000000U
#define DSP_STATUS_ADC_LATE     0x00000001U
#define DSP_STATUS_ADC_MISSED   0x00000002U

static volatile uint32_t adc1_frame_count = 0U;
static volatile uint32_t adc2_frame_count = 0U;
static uint32_t adc1_ready_timestamp_us = 0U;
static uint32_t adc2_ready_timestamp_us = 0U;
static volatile uint32_t adc_overrun_count = 0U;
static volatile uint32_t adc_missed_count = 0U;
static uint32_t dsp_sequence = 0U;
static uint32_t normal_stream_last_request_timestamp_us = 0U;

typedef struct
{
	int64_t integrator[CIC_STAGE_COUNT];
	int64_t comb_delay[CIC_STAGE_COUNT];
	uint32_t decimation_count;
} cic_filter_t;

typedef struct
{
	float b0;
	float b1;
	float b2;
	float a1;
	float a2;
	float x1;
	float x2;
	float y1;
	float y2;
} notch_filter_t;

static cic_filter_t adc1_filters[ADC_CH_PER_ADC];
static cic_filter_t adc2_filters[ADC_CH_PER_ADC];
static notch_filter_t adc1_notch_filters[ADC_CH_PER_ADC][NOTCH_FILTER_COUNT];
static notch_filter_t adc2_notch_filters[ADC_CH_PER_ADC][NOTCH_FILTER_COUNT];

__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc1_buf[ADC_BUF_LEN];
__attribute__((section(".dma_buffer"), aligned(32)))
uint16_t adc2_buf[ADC_BUF_LEN];

/** @brief Translate a sample/rank pair into the flattened DMA buffer index. */
static uint32_t get_adc_buffer_index(uint32_t sample, uint32_t channel);
/** @brief Convert the raw VREFINT reading into a VDDA estimate in millivolts. */
static uint32_t calculate_vdda_mv(uint16_t vref_raw);
/** @brief Initialize a packet header before populating it with fresh ADC data. */
static void prepare_tx_packet(tx_packet_t *packet);
/** @brief Convert one ready DMA frame into raw and filtered engineering values. */
static void fill_tx_packet_from_dma(tx_packet_t *packet);
/** @brief Reset the state of the CIC decimator bank. */
static void reset_cic_filters(cic_filter_t *filters, uint32_t filter_count);
/** @brief Push one raw ADC sample through a CIC decimator stage chain. */
static uint8_t cic_filter_process(cic_filter_t *filter, uint16_t sample, uint16_t *output);
/** @brief Scale and clamp a CIC output accumulator into a 16-bit ADC code. */
static uint16_t scale_cic_output(int64_t value);
/** @brief Reset and initialize the per-channel harmonic notch filters. */
static void reset_notch_filters(notch_filter_t filters[ADC_CH_PER_ADC][NOTCH_FILTER_COUNT]);
/** @brief Configure one IIR notch filter biquad for the selected tone frequency. */
static void initialize_notch_filter(notch_filter_t *filter, float notch_frequency_hz);
/** @brief Apply the configured cascade of notch filters to one channel sample. */
static uint32_t apply_notch_filters(notch_filter_t *filters, uint32_t filter_count, uint32_t sample_mv);
/** @brief Clamp a floating-point filter result back to the millivolt domain. */
static uint32_t clamp_filtered_mv(float value_mv);
/** @brief Record the latest ADC1 frame-ready timestamp. */
static void store_adc1_ready_timestamp_us(uint32_t timestamp_us);
/** @brief Record the latest ADC2 frame-ready timestamp. */
static void store_adc2_ready_timestamp_us(uint32_t timestamp_us);
/** @brief Return the newer of the ADC1 and ADC2 frame-ready timestamps. */
static uint32_t get_latest_sample_ready_timestamp_us(void);

void sampling_pipeline_service_reset(void)
{
	dsp_sequence = 0U;
	adc1_frame_count = 0U;
	adc2_frame_count = 0U;
	adc_overrun_count = 0U;
	adc_missed_count = 0U;
	reset_cic_filters(adc1_filters, ADC_CH_PER_ADC);
	reset_cic_filters(adc2_filters, ADC_CH_PER_ADC);
	reset_notch_filters(adc1_notch_filters);
	reset_notch_filters(adc2_notch_filters);
	normal_stream_last_request_timestamp_us = 0U;
	store_adc1_ready_timestamp_us(0U);
	store_adc2_ready_timestamp_us(0U);
}

void sampling_pipeline_service_start(void)
{
	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_buf, ADC_BUF_LEN) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_buf, ADC_BUF_LEN) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_LPTIM_PWM_Start(&hlptim1, ADC_TRIGGER_PERIOD, ADC_TRIGGER_PULSE) != HAL_OK)
	{
		Error_Handler();
	}

	osDelay(DSP_STARTUP_DELAY_MS);

	if (HAL_LPTIM_Counter_Start_IT(&hlptim2, DSP_TICK_PERIOD) != HAL_OK)
	{
		Error_Handler();
	}
}

void sampling_pipeline_service_process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame)
{
	uint32_t adc1_now = adc1_frame_count;
	uint32_t adc2_now = adc2_frame_count;
	uint32_t adc1_delta = adc1_now - *last_adc1_frame;
	uint32_t adc2_delta = adc2_now - *last_adc2_frame;
	tx_packet_t *working_tx_packet;

	tx_packet_service_reclaim_returned_packets();
	working_tx_packet = tx_packet_service_get_working_packet();
	if (working_tx_packet == NULL)
	{
		Error_Handler();
	}

	prepare_tx_packet(working_tx_packet);

	if ((adc1_delta == 1U) && (adc2_delta == 1U))
	{
		fill_tx_packet_from_dma(working_tx_packet);
		working_tx_packet->sample_ready_timestamp_us = get_latest_sample_ready_timestamp_us();
		*last_adc1_frame = adc1_now;
		*last_adc2_frame = adc2_now;
		tx_packet_service_publish_working_packet();
		if ((transmit_service_is_high_rate_capture_active() == 0U) &&
				((normal_stream_last_request_timestamp_us == 0U) ||
				((working_tx_packet->sample_ready_timestamp_us - normal_stream_last_request_timestamp_us) >= NORMAL_STREAM_PERIOD_US)))
		{
			normal_stream_last_request_timestamp_us = working_tx_packet->sample_ready_timestamp_us;
			transmit_service_request_send();
		}
		transmit_service_capture_high_rate_frame(working_tx_packet);
		return;
	}

	if ((adc1_delta == 0U) || (adc2_delta == 0U))
	{
		adc_overrun_count++;
		working_tx_packet->status_flags |= DSP_STATUS_ADC_LATE;
		return;
	}

	adc_missed_count++;
	*last_adc1_frame = adc1_now;
	*last_adc2_frame = adc2_now;
	working_tx_packet->status_flags |= DSP_STATUS_ADC_MISSED;
}

void sampling_pipeline_service_record_adc_frame_ready(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		store_adc1_ready_timestamp_us(tim2_get_timestamp_us());
		adc1_frame_count++;
	}
	else if (hadc->Instance == ADC2)
	{
		store_adc2_ready_timestamp_us(tim2_get_timestamp_us());
		adc2_frame_count++;
	}
}

static uint32_t get_adc_buffer_index(uint32_t sample, uint32_t channel)
{
	return (sample * ADC_CH_PER_ADC) + channel;
}

static uint32_t calculate_vdda_mv(uint16_t vref_raw)
{
	if (vref_raw == 0U)
	{
		return 0U;
	}

	return ((uint32_t)(*VREFINT_CAL_ADDR) * 3300UL) / (uint32_t)vref_raw;
}

static void prepare_tx_packet(tx_packet_t *packet)
{
	memset(packet, 0, sizeof(*packet));
	packet->sequence = ++dsp_sequence;
	packet->timestamp = dsp_sequence;
	packet->status_flags = DSP_STATUS_OK;
}

static void fill_tx_packet_from_dma(tx_packet_t *packet)
{
	uint16_t filtered_adc1_raw[ADC_CH_PER_ADC] = { 0U };
	uint16_t filtered_adc2_raw[ADC_CH_PER_ADC] = { 0U };
	uint16_t filtered_temp_raw = 0U;
	uint16_t filtered_vref_raw = 0U;
	uint8_t filtered_ready = 1U;

	for (uint32_t sample = 0U; sample < ADC_SAMPLES_PER_DSP; sample++)
	{
		uint32_t vref_index = get_adc_buffer_index(sample, ADC2_VREF_INDEX);
		uint32_t temp_index = get_adc_buffer_index(sample, ADC2_TEMP_INDEX);
		uint16_t raw_vref = adc2_buf[vref_index];
		uint16_t raw_temp = adc2_buf[temp_index];

		packet->vdda_mv[sample] = calculate_vdda_mv(raw_vref);
		if (packet->vdda_mv[sample] == 0U)
		{
			packet->status_flags |= DSP_STATUS_ADC_LATE;
		}

		for (uint32_t channel = 0U; channel < ADC_CH_PER_ADC; channel++)
		{
			uint32_t adc_index = get_adc_buffer_index(sample, channel);
			uint16_t raw_adc1 = adc1_buf[adc_index];
			uint16_t raw_adc2 = adc2_buf[adc_index];
			uint8_t adc1_ready;
			uint8_t adc2_ready;

			packet->adc1_mv[sample][channel] = __HAL_ADC_CALC_DATA_TO_VOLTAGE(
					packet->vdda_mv[sample],
					raw_adc1,
					ADC_RESOLUTION_16B);

			packet->adc2_mv[sample][channel] = __HAL_ADC_CALC_DATA_TO_VOLTAGE(
					packet->vdda_mv[sample],
					raw_adc2,
					ADC_RESOLUTION_16B);

			adc1_ready = cic_filter_process(&adc1_filters[channel], raw_adc1, &filtered_adc1_raw[channel]);
			adc2_ready = cic_filter_process(&adc2_filters[channel], raw_adc2, &filtered_adc2_raw[channel]);

			if (((sample + 1U) == ADC_SAMPLES_PER_DSP) && ((adc1_ready == 0U) || (adc2_ready == 0U)))
			{
				filtered_ready = 0U;
			}
		}

		packet->temperature_c[sample] = __HAL_ADC_CALC_TEMPERATURE(
				packet->vdda_mv[sample],
				raw_temp,
				ADC_RESOLUTION_16B);
	}

	filtered_temp_raw = filtered_adc2_raw[ADC2_TEMP_INDEX];
	filtered_vref_raw = filtered_adc2_raw[ADC2_VREF_INDEX];

	if (filtered_ready != 0U)
	{
		packet->filtered_vdda_mv[0] = calculate_vdda_mv(filtered_vref_raw);
		if (packet->filtered_vdda_mv[0] == 0U)
		{
			packet->status_flags |= DSP_STATUS_ADC_LATE;
		}

		for (uint32_t channel = 0U; channel < ADC_CH_PER_ADC; channel++)
		{
			packet->filtered_adc1_mv[0][channel] = __HAL_ADC_CALC_DATA_TO_VOLTAGE(
					packet->filtered_vdda_mv[0],
					filtered_adc1_raw[channel],
					ADC_RESOLUTION_16B);
			packet->filtered_adc1_mv[0][channel] = apply_notch_filters(
					adc1_notch_filters[channel],
					NOTCH_FILTER_COUNT,
					packet->filtered_adc1_mv[0][channel]);

			packet->filtered_adc2_mv[0][channel] = __HAL_ADC_CALC_DATA_TO_VOLTAGE(
					packet->filtered_vdda_mv[0],
					filtered_adc2_raw[channel],
					ADC_RESOLUTION_16B);
			packet->filtered_adc2_mv[0][channel] = apply_notch_filters(
					adc2_notch_filters[channel],
					NOTCH_FILTER_COUNT,
					packet->filtered_adc2_mv[0][channel]);
		}

		packet->filtered_temperature_c[0] = __HAL_ADC_CALC_TEMPERATURE(
				packet->filtered_vdda_mv[0],
				filtered_temp_raw,
				ADC_RESOLUTION_16B);
	}
	else
	{
		packet->status_flags |= DSP_STATUS_ADC_MISSED;
	}
}

static void reset_cic_filters(cic_filter_t *filters, uint32_t filter_count)
{
	memset(filters, 0, sizeof(*filters) * filter_count);
}

static void reset_notch_filters(notch_filter_t filters[ADC_CH_PER_ADC][NOTCH_FILTER_COUNT])
{
	static const float notch_frequencies_hz[NOTCH_FILTER_COUNT] = { 100.0f, 200.0f, 300.0f };

	for (uint32_t channel = 0U; channel < ADC_CH_PER_ADC; channel++)
	{
		for (uint32_t notch = 0U; notch < NOTCH_FILTER_COUNT; notch++)
		{
			initialize_notch_filter(&filters[channel][notch], notch_frequencies_hz[notch]);
		}
	}
}

static uint8_t cic_filter_process(cic_filter_t *filter, uint16_t sample, uint16_t *output)
{
	int64_t comb_value;

	filter->integrator[0] += (int64_t)sample;
	for (uint32_t stage = 1U; stage < CIC_STAGE_COUNT; stage++)
	{
		filter->integrator[stage] += filter->integrator[stage - 1U];
	}

	filter->decimation_count++;
	if (filter->decimation_count < CIC_DECIMATION_FACTOR)
	{
		return 0U;
	}

	filter->decimation_count = 0U;
	comb_value = filter->integrator[CIC_STAGE_COUNT - 1U];

	for (uint32_t stage = 0U; stage < CIC_STAGE_COUNT; stage++)
	{
		int64_t delayed_value = filter->comb_delay[stage];

		filter->comb_delay[stage] = comb_value;
		comb_value -= delayed_value;
	}

	*output = scale_cic_output(comb_value);
	return 1U;
}

static uint16_t scale_cic_output(int64_t value)
{
	value = (value + (1LL << (CIC_GAIN_SHIFT - 1U))) >> CIC_GAIN_SHIFT;

	if (value <= 0)
	{
		return 0U;
	}

	if (value >= (int64_t)UINT16_MAX)
	{
		return UINT16_MAX;
	}

	return (uint16_t)value;
}

static void initialize_notch_filter(notch_filter_t *filter, float notch_frequency_hz)
{
	float omega = 2.0f * NOTCH_PI * notch_frequency_hz / DSP_OUTPUT_SAMPLE_RATE_HZ;
	float alpha = sinf(omega) / (2.0f * NOTCH_FILTER_Q);
	float cos_omega = cosf(omega);
	float a0 = 1.0f + alpha;

	memset(filter, 0, sizeof(*filter));
	filter->b0 = 1.0f / a0;
	filter->b1 = (-2.0f * cos_omega) / a0;
	filter->b2 = 1.0f / a0;
	filter->a1 = (-2.0f * cos_omega) / a0;
	filter->a2 = (1.0f - alpha) / a0;
}

static uint32_t apply_notch_filters(notch_filter_t *filters, uint32_t filter_count, uint32_t sample_mv)
{
	float value = (float)sample_mv;

	for (uint32_t index = 0U; index < filter_count; index++)
	{
		notch_filter_t *filter = &filters[index];
		float output = (filter->b0 * value) +
				(filter->b1 * filter->x1) +
				(filter->b2 * filter->x2) -
				(filter->a1 * filter->y1) -
				(filter->a2 * filter->y2);

		filter->x2 = filter->x1;
		filter->x1 = value;
		filter->y2 = filter->y1;
		filter->y1 = output;
		value = output;
	}

	return clamp_filtered_mv(value);
}

static uint32_t clamp_filtered_mv(float value_mv)
{
	if (value_mv <= 0.0f)
	{
		return 0U;
	}

	if (value_mv >= (float)UINT32_MAX)
	{
		return UINT32_MAX;
	}

	return (uint32_t)(value_mv + 0.5f);
}

static void store_adc1_ready_timestamp_us(uint32_t timestamp_us)
{
	__atomic_store_n(&adc1_ready_timestamp_us, timestamp_us, __ATOMIC_RELEASE);
}

static void store_adc2_ready_timestamp_us(uint32_t timestamp_us)
{
	__atomic_store_n(&adc2_ready_timestamp_us, timestamp_us, __ATOMIC_RELEASE);
}

static uint32_t get_latest_sample_ready_timestamp_us(void)
{
	uint32_t adc1_timestamp_us = __atomic_load_n(&adc1_ready_timestamp_us, __ATOMIC_ACQUIRE);
	uint32_t adc2_timestamp_us = __atomic_load_n(&adc2_ready_timestamp_us, __ATOMIC_ACQUIRE);

	if (adc1_timestamp_us >= adc2_timestamp_us)
	{
		return adc1_timestamp_us;
	}

	return adc2_timestamp_us;
}