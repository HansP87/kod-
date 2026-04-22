#ifndef APP_TASK_FLAGS_H
#define APP_TASK_FLAGS_H

/**
 * @file app_task_flags.h
 * @brief CMSIS-RTOS thread-flag bits shared across application tasks.
 * @ingroup task_entrypoints
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Thread flag raised by the scheduler timer to release the DSP task. */
#define DSP_FLAG_TICK           (1U << 0)
/** @brief Thread flag raised to request a UART packet transmission. */
#define TX_FLAG_SEND            (1U << 0)

#ifdef __cplusplus
}
#endif

#endif