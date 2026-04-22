#ifndef MONITOR_SERVICE_H
#define MONITOR_SERVICE_H

/**
 * @file monitor_service.h
 * @brief Boot-banner and button-trigger monitor helpers.
 * @ingroup monitor_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

/** @brief Reset the monitor state used for startup banners and button notifications. */
void monitor_service_initialize(void);

/** @brief Notify the monitor service that the user button was pressed. */
void monitor_service_notify_button_press(void);

/**
 * @brief Advance monitor-service state and request any pending UART output.
 * @param transmit_task_handle Transmit task to wake when a monitor frame should be sent.
 */
void monitor_service_process_tick(osThreadId_t transmit_task_handle);

#ifdef __cplusplus
}
#endif

#endif