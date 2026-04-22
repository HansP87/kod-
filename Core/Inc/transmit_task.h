#ifndef TRANSMIT_TASK_H
#define TRANSMIT_TASK_H

/**
 * @file transmit_task.h
 * @brief Entry point for the USART transmit worker thread.
 * @ingroup task_entrypoints
 */

/** @brief Run the UART transmit thread that serializes and sends the latest packet. */
void start_transmit_task(void *argument);

#endif
