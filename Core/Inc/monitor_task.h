#ifndef MONITOR_TASK_H
#define MONITOR_TASK_H

/**
 * @file monitor_task.h
 * @brief Entry point for the monitor thread.
 * @ingroup task_entrypoints
 */

/** @brief Run the monitor thread that emits startup banners and auto-trigger beacons. */
void start_monitor_task(void *argument);

#endif
