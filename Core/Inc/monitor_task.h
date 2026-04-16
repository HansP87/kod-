#ifndef MONITOR_TASK_H
#define MONITOR_TASK_H

/**
 * @brief Run the monitor thread that emits startup banners and auto-trigger beacons.
 * @param argument Unused CMSIS-RTOS thread argument.
 */
void start_monitor_task(void *argument);

#endif
