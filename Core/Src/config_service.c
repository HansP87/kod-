#include <string.h>
#include "config_service.h"
#include "flash_config_storage_bsp.h"

#define CONFIG_SERVICE_DEFAULT_PRODUCT_NAME    "devADC-dk"

typedef struct
{
	char product_name[CONFIG_SERVICE_PRODUCT_NAME_MAX_LEN + 1U];
} runtime_app_config_t;

static runtime_app_config_t current_app_config;

static void config_service_load_defaults(void);
static config_service_status_t config_service_validate_product_name(const char *product_name);

void config_service_init(void)
{
	flash_config_storage_data_t stored_config;

	config_service_load_defaults();

	if (flash_config_storage_bsp_load(&stored_config) != FLASH_CONFIG_STORAGE_STATUS_OK)
	{
		return;
	}

	if (config_service_validate_product_name(stored_config.product_name) != CONFIG_SERVICE_STATUS_OK)
	{
		return;
	}

	(void)memcpy(current_app_config.product_name, stored_config.product_name, sizeof(current_app_config.product_name));
	current_app_config.product_name[sizeof(current_app_config.product_name) - 1U] = '\0';
}

const char *config_service_get_product_name(void)
{
	return current_app_config.product_name;
}

config_service_status_t config_service_set_product_name(const char *product_name)
{
	config_service_status_t status = config_service_validate_product_name(product_name);

	if (status != CONFIG_SERVICE_STATUS_OK)
	{
		return status;
	}

	(void)strncpy(current_app_config.product_name, product_name, sizeof(current_app_config.product_name) - 1U);
	current_app_config.product_name[sizeof(current_app_config.product_name) - 1U] = '\0';
	return CONFIG_SERVICE_STATUS_OK;
}

config_service_status_t config_service_save(void)
{
	flash_config_storage_data_t stored_config;
	flash_config_storage_bsp_status_t storage_status;

	(void)memcpy(stored_config.product_name, current_app_config.product_name, sizeof(stored_config.product_name));
	stored_config.product_name[sizeof(stored_config.product_name) - 1U] = '\0';

	storage_status = flash_config_storage_bsp_save(&stored_config);
	if (storage_status == FLASH_CONFIG_STORAGE_STATUS_FLASH_ERROR)
	{
		return CONFIG_SERVICE_STATUS_FLASH_ERROR;
	}

	if (storage_status != FLASH_CONFIG_STORAGE_STATUS_OK)
	{
		return CONFIG_SERVICE_STATUS_INVALID_ARGUMENT;
	}

	return CONFIG_SERVICE_STATUS_OK;
}

static void config_service_load_defaults(void)
{
	(void)strncpy(
			current_app_config.product_name,
			CONFIG_SERVICE_DEFAULT_PRODUCT_NAME,
			sizeof(current_app_config.product_name) - 1U);
	current_app_config.product_name[sizeof(current_app_config.product_name) - 1U] = '\0';
}

static config_service_status_t config_service_validate_product_name(const char *product_name)
{
	size_t index;
	size_t length;

	if (product_name == NULL)
	{
		return CONFIG_SERVICE_STATUS_INVALID_ARGUMENT;
	}

	length = strlen(product_name);
	if ((length == 0U) || (length > CONFIG_SERVICE_PRODUCT_NAME_MAX_LEN))
	{
		return CONFIG_SERVICE_STATUS_INVALID_ARGUMENT;
	}

	for (index = 0U; index < length; index++)
	{
		uint8_t byte = (uint8_t)product_name[index];

		if ((byte < 0x20U) || (byte > 0x7EU) || (byte == ','))
		{
			return CONFIG_SERVICE_STATUS_INVALID_ARGUMENT;
		}
	}

	return CONFIG_SERVICE_STATUS_OK;
}