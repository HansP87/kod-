#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

/**
 * @file command_task.h
 * @brief UART command task entry point and flag definitions.
 * @ingroup task_entrypoints
 */

#include <stdint.h>

/** Thread flag raised when one or more UART RX bytes are queued for the command task. */
#define COMMAND_TASK_FLAG_RX          (1U << 0)

/** @brief Run the UART command reception and configuration-mode state machine. */
void start_command_task(void *argument);

#endif