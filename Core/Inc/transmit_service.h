#ifndef TRANSMIT_SERVICE_H
#define TRANSMIT_SERVICE_H

/**
 * @file transmit_service.h
 * @brief Packet transmission and high-rate capture control.
 * @ingroup transport_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "app_types.h"

/** @brief Reset transmit state, including the live high-rate capture session. */
void transmit_service_reset(void);

/** @brief Request that the transmit task send the latest available packet. */
void transmit_service_request_send(void);

/** @brief Process one transmit request from the transmit task context. */
void transmit_service_process_send_request(void);

/**
 * @brief Start a count-bounded high-rate binary capture session.
 * @param frame_count Number of filtered frames to emit.
 * @return Non-zero when the capture was started.
 */
uint32_t transmit_service_start_high_rate_capture(uint32_t frame_count);

/**
 * @brief Check whether a high-rate capture session is currently active.
 * @return Non-zero when high-rate capture is active.
 */
uint32_t transmit_service_is_high_rate_capture_active(void);

/**
 * @brief Offer a newly prepared packet to the live high-rate capture path.
 * @param packet Latest packet prepared by the DSP pipeline.
 */
void transmit_service_capture_high_rate_frame(const tx_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif