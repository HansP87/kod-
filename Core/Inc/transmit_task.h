#ifndef TRANSMIT_TASK_H
#define TRANSMIT_TASK_H

/**
 * @brief Run the UART transmit thread that serializes and sends the latest packet.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_transmit_task(void *argument);

#endif
