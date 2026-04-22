#ifndef DEFAULT_TASK_H
#define DEFAULT_TASK_H

/**
 * @file default_task.h
 * @brief Entry point for the low-priority background task.
 * @ingroup task_entrypoints
 */

/** @brief Run the default background thread reserved for future work. */
void start_default_task(void *argument);

#endif
