#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "main.h"

/** Number of conversion channels captured per ADC peripheral. */
#define ADC_CH_PER_ADC          4U
/** Number of samples aggregated into each transmitted packet. */
#define ADC_SAMPLES_PER_DSP     2U

/** Thread flag raised by the scheduler timer to release the DSP task. */
#define DSP_FLAG_TICK           (1U << 0)
/** Thread flag raised to request a UART packet transmission. */
#define TX_FLAG_SEND            (1U << 0)

/** Application operating modes that gate the streaming path. */
typedef enum
{
	APP_MODE_STREAMING = 0,
	APP_MODE_CONFIG = 1,
} app_mode_t;

/**
 * @brief Aggregated ADC payload published by the DSP pipeline.
 */
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

/**
 * @brief Reset the application runtime counters and mode state.
 */
void app_runtime_reset(void);

/**
 * @brief Initialize the internal ownership model for transmit packet buffers.
 */
void app_runtime_initialize_tx_buffer_ownership(void);

/**
 * @brief Start ADC DMA acquisition and timer-driven sampling.
 */
void app_runtime_start_sampling_pipeline(void);

/**
 * @brief Process one DSP scheduling tick and publish a new packet when data is ready.
 * @param last_adc1_frame Last ADC1 frame counter observed by the DSP task.
 * @param last_adc2_frame Last ADC2 frame counter observed by the DSP task.
 */
void app_runtime_process_dsp_tick(uint32_t *last_adc1_frame, uint32_t *last_adc2_frame);

/**
 * @brief Claim the latest published packet for transmission.
 * @param current_tx_packet Pointer to the caller-owned packet pointer that will receive the latest packet.
 */
void app_runtime_claim_latest_tx_packet(tx_packet_t **current_tx_packet);

/**
 * @brief Compute the age of a packet relative to the latest trigger timestamp.
 * @param packet Packet whose readiness timestamp should be compared.
 * @return Packet latency in microseconds.
 */
uint32_t app_runtime_get_sample_latency_us(const tx_packet_t *packet);

/**
 * @brief Serialize a packet into the UART text frame format used by the application.
 * @param pkt Packet to serialize.
 * @param sample_latency_us Computed latency associated with the packet.
 * @param buf Destination buffer.
 * @param size Size of the destination buffer in bytes.
 * @return Number of bytes written, or `-1` if the buffer is too small.
 */
int app_runtime_serialize_tx_packet(const tx_packet_t *pkt, uint32_t sample_latency_us, char *buf, size_t size);

/**
 * @brief Store the timestamp of the latest trigger event used for latency reporting.
 * @param timestamp_us Trigger timestamp in microseconds.
 */
void app_runtime_set_button_event_timestamp_us(uint32_t timestamp_us);

/**
 * @brief Record that an ADC DMA frame completed.
 * @param hadc ADC instance that completed its current frame.
 */
void app_runtime_record_adc_frame_ready(ADC_HandleTypeDef *hadc);

/**
 * @brief Snapshot the last transmitted packet for debugging purposes.
 * @param packet Packet that was just sent over UART.
 */
void app_runtime_store_last_sent_debug(const tx_packet_t *packet);

/**
 * @brief Update the application mode.
 * @param mode New operating mode.
 */
void app_runtime_set_mode(app_mode_t mode);

/**
 * @brief Read the current application mode.
 * @return Current operating mode.
 */
app_mode_t app_runtime_get_mode(void);

/**
 * @brief Check whether periodic streaming is currently enabled.
 * @return Non-zero when the application is in streaming mode, otherwise zero.
 */
uint32_t app_runtime_is_streaming_mode(void);

#ifdef __cplusplus
}
#endif

#endif
