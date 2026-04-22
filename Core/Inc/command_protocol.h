#ifndef COMMAND_PROTOCOL_H
#define COMMAND_PROTOCOL_H

/**
 * @file command_protocol.h
 * @brief CRC-checked ASCII framing helpers for configuration commands.
 * @ingroup command_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/** Maximum number of comma-separated parameters accepted in one command frame. */
#define COMMAND_PROTOCOL_MAX_PARAMETERS      6U

/** Parse status codes returned by the command framing layer. */
typedef enum
{
	/** Frame parsed successfully. */
	COMMAND_PROTOCOL_PARSE_OK = 0,
	/** Frame exceeded the supported maximum length. */
	COMMAND_PROTOCOL_PARSE_TOO_LONG = 1,
	/** Frame structure was malformed. */
	COMMAND_PROTOCOL_PARSE_MALFORMED = 2,
	/** CRC8 check failed. */
	COMMAND_PROTOCOL_PARSE_BADCRC = 3,
	/** Request sign was not the expected host-to-device sign. */
	COMMAND_PROTOCOL_PARSE_BADSIGN = 4,
} command_protocol_parse_status_t;

/** Parsed configuration request view into a mutable command buffer. */
typedef struct
{
	/** Null-terminated command name token. */
	const char *command_name;
	/** Null-terminated parameter tokens in the mutable frame buffer. */
	char *parameters[COMMAND_PROTOCOL_MAX_PARAMETERS];
	/** Number of valid entries in @ref parameters. */
	uint32_t parameter_count;
} command_protocol_request_t;

/**
 * @brief Parse a framed ASCII configuration request.
 * @param frame Incoming immutable frame text.
 * @param mutable_frame Scratch buffer used to hold tokenized fields.
 * @param mutable_frame_size Size of @p mutable_frame in bytes.
 * @param request Parsed request result.
 * @return Parse status.
 */
command_protocol_parse_status_t command_protocol_parse_frame(
		const char *frame,
		char *mutable_frame,
		size_t mutable_frame_size,
		command_protocol_request_t *request);

/**
 * @brief Convert a parse status code into the exported protocol error string.
 * @param status Parse status to translate.
 * @return Constant error-code string.
 */
const char *command_protocol_get_error_code(command_protocol_parse_status_t status);

/**
 * @brief Build a minimal framed ASCII response.
 * @param command_name Upper-case response command name.
 * @param parameter0 Optional first response parameter, or `NULL` when absent.
 * @param frame Destination buffer for the framed response.
 * @param frame_size Size of @p frame in bytes.
 * @return Number of bytes written, or a negative value on failure.
 */
int command_protocol_build_response(const char *command_name, const char *parameter0, char *frame, size_t frame_size);

#ifdef __cplusplus
}
#endif

#endif