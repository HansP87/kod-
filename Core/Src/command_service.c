#include <stdio.h>
#include <string.h>
#include "app_mode_service.h"
#include "command_protocol.h"
#include "command_service.h"
#include "config_service.h"
#include "flash_config_storage_bsp.h"
#include "main.h"

#define COMMAND_SERVICE_FRAME_BUFFER_LEN    168U

static uint32_t build_error_response(const char *error_code, char *response, size_t response_size);
static uint32_t build_product_name_response(char *response, size_t response_size);
static uint32_t handle_product_name_command(const command_protocol_request_t *request, char *response, size_t response_size);
static uint32_t handle_save_command(const command_protocol_request_t *request, char *response, size_t response_size);

uint32_t command_service_process_line(
		const char *line,
		char *response,
		size_t response_size,
		command_service_action_t *action)
{
	char mutable_frame[COMMAND_SERVICE_FRAME_BUFFER_LEN];
	command_protocol_request_t request;
	command_protocol_parse_status_t parse_status;

	if ((line == NULL) || (response == NULL) || (action == NULL))
	{
		return 0U;
	}

	*action = COMMAND_SERVICE_ACTION_NONE;

	if (strcmp(line, "CONFIGMODE") == 0)
	{
		app_mode_service_set_mode(APP_MODE_CONFIG);
		(void)snprintf(response, response_size, "ENTERED CONFIGMODE\n");
		return 1U;
	}

	if (app_mode_service_get_mode() != APP_MODE_CONFIG)
	{
		return 0U;
	}

	parse_status = command_protocol_parse_frame(line, mutable_frame, sizeof(mutable_frame), &request);
	if (parse_status != COMMAND_PROTOCOL_PARSE_OK)
	{
		return build_error_response(command_protocol_get_error_code(parse_status), response, response_size);
	}

	if (strcmp(request.command_name, "mcu_serial") == 0)
	{
		char serial_text[25];

		if (request.parameter_count != 0U)
		{
			return build_error_response("MALFORMED", response, response_size);
		}

		(void)snprintf(
				serial_text,
				sizeof(serial_text),
				"%08lX%08lX%08lX",
				(unsigned long)HAL_GetUIDw0(),
				(unsigned long)HAL_GetUIDw1(),
				(unsigned long)HAL_GetUIDw2());
		return (uint32_t)(command_protocol_build_response("MCU_SERIAL", serial_text, response, response_size) > 0);
	}

	if (strcmp(request.command_name, "product_name") == 0)
	{
		return handle_product_name_command(&request, response, response_size);
	}

	if (strcmp(request.command_name, "save") == 0)
	{
		return handle_save_command(&request, response, response_size);
	}

	if (strcmp(request.command_name, "exit_config") == 0)
	{
		if (request.parameter_count != 0U)
		{
			return build_error_response("MALFORMED", response, response_size);
		}

		app_mode_service_set_mode(APP_MODE_STREAMING);
		return (uint32_t)(command_protocol_build_response("EXIT_CONFIG", "STREAMING", response, response_size) > 0);
	}

	if (strcmp(request.command_name, "reset") == 0)
	{
		if (request.parameter_count != 0U)
		{
			return build_error_response("MALFORMED", response, response_size);
		}

		*action = COMMAND_SERVICE_ACTION_REBOOT;
		return (uint32_t)(command_protocol_build_response("RESET", "REBOOTING", response, response_size) > 0);
	}

	return build_error_response("UNKNOWN_COMMAND", response, response_size);
}

static uint32_t build_error_response(const char *error_code, char *response, size_t response_size)
{
	return (uint32_t)(command_protocol_build_response("ERROR", error_code, response, response_size) > 0);
}

static uint32_t build_product_name_response(char *response, size_t response_size)
{
	return (uint32_t)(command_protocol_build_response("PRODUCT_NAME", config_service_get_product_name(), response, response_size) > 0);
}

static uint32_t handle_product_name_command(const command_protocol_request_t *request, char *response, size_t response_size)
{
	config_service_status_t status;

	if (request->parameter_count == 0U)
	{
		return build_product_name_response(response, response_size);
	}

	if (request->parameter_count != 1U)
	{
		return build_error_response("MALFORMED", response, response_size);
	}

	status = config_service_set_product_name(request->parameters[0]);
	if (status != CONFIG_SERVICE_STATUS_OK)
	{
		return build_error_response("BADPARAM", response, response_size);
	}

	return build_product_name_response(response, response_size);
}

static uint32_t handle_save_command(const command_protocol_request_t *request, char *response, size_t response_size)
{
	char error_text[32];
	config_service_status_t status;

	if (request->parameter_count != 0U)
	{
		return build_error_response("MALFORMED", response, response_size);
	}

	status = config_service_save();
	if (status == CONFIG_SERVICE_STATUS_FLASH_ERROR)
	{
		(void)snprintf(
				error_text,
				sizeof(error_text),
				"SAVE_%08lX_%lu",
				(unsigned long)flash_config_storage_bsp_get_last_flash_error(),
				(unsigned long)flash_config_storage_bsp_get_last_sector_error());
		return build_error_response(error_text, response, response_size);
	}

	if (status == CONFIG_SERVICE_STATUS_VERIFY_ERROR)
	{
		return build_error_response("SAVE_VERIFY", response, response_size);
	}

	return (uint32_t)(command_protocol_build_response("SAVE", "OK", response, response_size) > 0);
}