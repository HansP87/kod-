#ifndef APP_MODE_SERVICE_H
#define APP_MODE_SERVICE_H

/**
 * @file app_mode_service.h
 * @brief Accessors for the application operating mode and button-event timestamp.
 * @ingroup app_core
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "app_types.h"

/**
 * @brief Select the active application mode.
 * @param mode Mode to make active.
 */
void app_mode_service_set_mode(app_mode_t mode);

/**
 * @brief Read the currently active application mode.
 * @return Current application mode.
 */
app_mode_t app_mode_service_get_mode(void);

/**
 * @brief Check whether the application is in streaming mode.
 * @return Non-zero when streaming mode is active.
 */
uint32_t app_mode_service_is_streaming_mode(void);

/**
 * @brief Store the timestamp of the latest button event.
 * @param timestamp_us Event timestamp in microseconds.
 */
void app_mode_service_set_button_event_timestamp_us(uint32_t timestamp_us);

/**
 * @brief Read the timestamp of the latest button event.
 * @return Last stored button-event timestamp in microseconds.
 */
uint32_t app_mode_service_get_button_event_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif