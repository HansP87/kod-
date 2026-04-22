/**
 * @file command_protocol.c
 * @brief Parsing and framing helpers for CRC-protected ASCII configuration commands.
 * @ingroup command_services
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command_protocol.h"

#define COMMAND_REQUEST_SIGN            '@'
#define COMMAND_RESPONSE_SIGN           '!'
#define COMMAND_CRC8_POLYNOMIAL         0x07U
#define COMMAND_CRC8_SEED               0xA5U

/** @brief Compute the protocol CRC8 over a byte range. */
static uint8_t compute_crc8(const uint8_t *data, size_t length);
/** @brief Parse a one-byte CRC8 field encoded as hexadecimal text. */
static int parse_crc8_field(const char *text, uint8_t *value);

command_protocol_parse_status_t command_protocol_parse_frame(
		const char *frame,
		char *mutable_frame,
		size_t mutable_frame_size,
		command_protocol_request_t *request)
{
	char *tokens[COMMAND_PROTOCOL_MAX_PARAMETERS + 2U];
	char *cursor;
	char *last_comma;
	uint8_t received_crc = 0U;
	uint8_t expected_crc;
	uint32_t token_count = 0U;

	if ((frame == NULL) || (mutable_frame == NULL) || (request == NULL))
	{
		return COMMAND_PROTOCOL_PARSE_MALFORMED;
	}

	if (strlen(frame) >= mutable_frame_size)
	{
		return COMMAND_PROTOCOL_PARSE_TOO_LONG;
	}

	last_comma = strrchr(frame, ',');
	if (last_comma == NULL)
	{
		return COMMAND_PROTOCOL_PARSE_MALFORMED;
	}

	if (parse_crc8_field(last_comma + 1, &received_crc) == 0)
	{
		return COMMAND_PROTOCOL_PARSE_BADCRC;
	}

	expected_crc = compute_crc8((const uint8_t *)frame, (size_t)(last_comma - frame));
	if (expected_crc != received_crc)
	{
		return COMMAND_PROTOCOL_PARSE_BADCRC;
	}

	(void)strcpy(mutable_frame, frame);
	last_comma = strrchr(mutable_frame, ',');
	*last_comma = '\0';

	cursor = strtok(mutable_frame, ",");
	while ((cursor != NULL) && (token_count < (COMMAND_PROTOCOL_MAX_PARAMETERS + 2U)))
	{
		tokens[token_count] = cursor;
		token_count++;
		cursor = strtok(NULL, ",");
	}

	if (token_count < 2U)
	{
		return COMMAND_PROTOCOL_PARSE_MALFORMED;
	}

	if ((strlen(tokens[0]) != 1U) || (tokens[0][0] != COMMAND_REQUEST_SIGN))
	{
		return COMMAND_PROTOCOL_PARSE_BADSIGN;
	}

	request->command_name = tokens[1];
	request->parameter_count = token_count - 2U;
	for (uint32_t index = 0U; index < request->parameter_count; index++)
	{
		request->parameters[index] = tokens[index + 2U];
	}

	return COMMAND_PROTOCOL_PARSE_OK;
}

const char *command_protocol_get_error_code(command_protocol_parse_status_t status)
{
	switch (status)
	{
		case COMMAND_PROTOCOL_PARSE_TOO_LONG:
			return "TOO_LONG";
		case COMMAND_PROTOCOL_PARSE_MALFORMED:
			return "MALFORMED";
		case COMMAND_PROTOCOL_PARSE_BADCRC:
			return "BADCRC";
		case COMMAND_PROTOCOL_PARSE_BADSIGN:
			return "BADSIGN";
		case COMMAND_PROTOCOL_PARSE_OK:
		default:
			return "MALFORMED";
	}
}

int command_protocol_build_response(const char *command_name, const char *parameter0, char *frame, size_t frame_size)
{
	char payload[160];
	uint8_t crc;

	if ((command_name == NULL) || (frame == NULL) || (frame_size == 0U))
	{
		return -1;
	}

	if (parameter0 != NULL)
	{
		if (snprintf(payload, sizeof(payload), "%c,%s,%s", COMMAND_RESPONSE_SIGN, command_name, parameter0) < 0)
		{
			return -1;
		}
	}
	else if (snprintf(payload, sizeof(payload), "%c,%s", COMMAND_RESPONSE_SIGN, command_name) < 0)
	{
		return -1;
	}

	crc = compute_crc8((const uint8_t *)payload, strlen(payload));
	return snprintf(frame, frame_size, "%s,%02X\n", payload, (unsigned int)crc);
}

static uint8_t compute_crc8(const uint8_t *data, size_t length)
{
	uint8_t crc = COMMAND_CRC8_SEED;

	for (size_t index = 0U; index < length; index++)
	{
		crc ^= data[index];
		for (uint32_t bit = 0U; bit < 8U; bit++)
		{
			if ((crc & 0x80U) != 0U)
			{
				crc = (uint8_t)((crc << 1U) ^ COMMAND_CRC8_POLYNOMIAL);
			}
			else
			{
				crc <<= 1U;
			}
		}
	}

	return crc;
}

static int parse_crc8_field(const char *text, uint8_t *value)
{
	char *parse_end;
	unsigned long parsed_value;

	if ((text == NULL) || (value == NULL) || (strlen(text) == 0U) || (strlen(text) > 2U))
	{
		return 0;
	}

	for (size_t index = 0U; text[index] != '\0'; index++)
	{
		if (isxdigit((unsigned char)text[index]) == 0)
		{
			return 0;
		}
	}

	parsed_value = strtoul(text, &parse_end, 16);
	if ((*parse_end != '\0') || (parsed_value > 0xFFU))
	{
		return 0;
	}

	*value = (uint8_t)parsed_value;
	return 1;
}