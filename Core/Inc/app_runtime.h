#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "main.h"

#define ADC_CH_PER_ADC          4U
#define ADC_SAMPLES_PER_DSP     2U

#define DSP_FLAG_TICK           (1U << 0)
#define TX_FLAG_SEND            (1U << 0)

typedef struct
{
	uint32_t sequence;
	uint32_t timestamp;
	uint32_t sample_ready_timestamp_us;

	uint32_t vdda_mv[ADC_SAMPLES_PER_DSP];
	int32_t temperature_c[ADC_SAMPLES_PER_DSP];

	uint32_t adc1_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];
	uint32_t adc2_mv[ADC_SAMPLES_PER_DSP][ADC_CH_PER_ADC];

	uint32_t status_flags;
} tx_packet_t;

void app_runtime_reset(void);
void app_runtime_initialize_tx_buffer_ownership(void);
void app_runtime_start_sampling_pipeline(void);
void app_runtime_process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame);
void app_runtime_claim_latest_tx_packet(tx_packet_t **current_tx_packet);
uint32_t app_runtime_get_sample_latency_us(const tx_packet_t *packet);
int app_runtime_serialize_tx_packet(const tx_packet_t *pkt, uint32_t sample_latency_us, char *buf, size_t size);
void app_runtime_set_button_event_timestamp_us(uint32_t timestamp_us);
void app_runtime_record_adc_frame_ready(ADC_HandleTypeDef *hadc);
void app_runtime_store_last_sent_debug(const tx_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
