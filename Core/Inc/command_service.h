#ifndef COMMAND_SERVICE_H
#define COMMAND_SERVICE_H

/**
 * @file command_service.h
 * @brief High-level command dispatcher for ASCII configuration commands.
 * @ingroup command_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/** Side effects requested by a parsed command. */
typedef enum
{
	/** No deferred system action is required. */
	COMMAND_SERVICE_ACTION_NONE = 0,
	/** Reboot the MCU after the response has been transmitted. */
	COMMAND_SERVICE_ACTION_REBOOT = 1,
} command_service_action_t;

/**
 * @brief Parse and execute one complete command line.
 * @param line Received command line without trailing newline.
 * @param response Output buffer for the framed ASCII response.
 * @param response_size Size of @p response in bytes.
 * @param action Optional deferred action selected by the command.
 * @return Non-zero when a response was written to @p response.
 */
uint32_t command_service_process_line(
		const char *line,
		char *response,
		size_t response_size,
		command_service_action_t *action);

#ifdef __cplusplus
}
#endif

#endif