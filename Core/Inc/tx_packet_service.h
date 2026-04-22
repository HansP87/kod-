#ifndef TX_PACKET_SERVICE_H
#define TX_PACKET_SERVICE_H

/**
 * @file tx_packet_service.h
 * @brief Ownership and serialization helpers for transmit packets.
 * @ingroup transport_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "app_types.h"

/** @brief Reset the transmit-packet ownership pools and debug state. */
void tx_packet_service_reset(void);

/** @brief Initialize packet ownership state during application startup. */
void tx_packet_service_initialize_ownership(void);

/**
 * @brief Get the packet currently reserved for DSP writes.
 * @return Pointer to the working packet.
 */
tx_packet_t *tx_packet_service_get_working_packet(void);

/** @brief Publish the current working packet and rotate in a fresh write buffer. */
void tx_packet_service_publish_working_packet(void);

/** @brief Reclaim packets returned by the transmit task back into the free pool. */
void tx_packet_service_reclaim_returned_packets(void);

/**
 * @brief Claim the newest published packet for transmission.
 * @param current_tx_packet Pointer to the caller-owned current transmit packet slot.
 */
void tx_packet_service_claim_latest_packet(tx_packet_t **current_tx_packet);

/**
 * @brief Compute packet age relative to the latest relevant trigger timestamp.
 * @param packet Packet for which latency should be computed.
 * @return Packet age in microseconds.
 */
uint32_t tx_packet_service_get_sample_latency_us(const tx_packet_t *packet);

/**
 * @brief Serialize a packet into the compact normal-stream wire format.
 * @param pkt Packet to serialize.
 * @param sample_latency_us Packet age to include in the wire payload.
 * @param buf Destination buffer.
 * @param size Size of @p buf in bytes.
 * @return Number of bytes written, or a negative value on failure.
 */
int tx_packet_service_serialize(const tx_packet_t *pkt, uint32_t sample_latency_us, char *buf, size_t size);

/**
 * @brief Copy the last transmitted packet into the debug snapshot slot.
 * @param packet Packet that was just transmitted.
 */
void tx_packet_service_store_last_sent_debug(const tx_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif