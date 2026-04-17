#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "app_config.h"
#include "main.h"
#include "stm32h7xx_hal_flash_ex.h"

#define APP_CONFIG_FLASH_ADDRESS           0x081FC000UL
#define APP_CONFIG_FLASH_SECTOR            FLASH_SECTOR_63
#define APP_CONFIG_FLASH_BANK              FLASH_BANK_2
#define APP_CONFIG_FLASH_VOLTAGE_RANGE     0U
#define APP_CONFIG_MAGIC                   0x43464731UL
#define APP_CONFIG_VERSION                 1UL
#define APP_CONFIG_DEFAULT_PRODUCT_NAME    "devADC-dk"
#define APP_CONFIG_FLASHWORD_SIZE          16U

typedef struct __attribute__((aligned(APP_CONFIG_FLASHWORD_SIZE)))
{
	/** Magic number used to detect initialized config records. */
	uint32_t magic;
	/** Schema version of the persisted record layout. */
	uint32_t version;
	/** Size of the payload covered by the current record version. */
	uint32_t payload_size;
	/** Reserved for future metadata fields. */
	uint32_t reserved0;
	/** Persisted product name including the terminator. */
	char product_name[APP_CONFIG_PRODUCT_NAME_MAX_LEN + 1U];
	/** CRC32 over the record fields that precede this member. */
	uint32_t crc32;
	/** Reserved padding to keep the record flashword-aligned. */
	uint32_t reserved1[3];
} persisted_app_config_t;

typedef struct
{
	char product_name[APP_CONFIG_PRODUCT_NAME_MAX_LEN + 1U];
} runtime_app_config_t;

/** Reset the runtime configuration to its built-in defaults. */
static void app_config_load_defaults(void);
/** Check whether a product name fits the delimiter-based UART protocol. */
static app_config_status_t validate_product_name(const char *product_name);
/** Compute the CRC32 used to protect the persisted configuration record. */
static uint32_t calculate_crc32(const uint8_t *data, size_t length);
/** Check whether a flash region is still in the erased state. */
static uint32_t is_flash_region_erased(const uint8_t *data, size_t length);
/** Build the flash record from the current in-memory configuration. */
static void build_persisted_record(persisted_app_config_t *record);
/** Validate and load a persisted record into the runtime configuration. */
static uint32_t load_record_if_valid(const persisted_app_config_t *record);

static runtime_app_config_t current_app_config;
static uint32_t last_flash_error = 0U;
static uint32_t last_sector_error = 0U;

/**
 * @brief Load the active configuration from flash or fall back to defaults.
 */
void app_config_init(void)
{
	const persisted_app_config_t *flash_record = (const persisted_app_config_t *)APP_CONFIG_FLASH_ADDRESS;

	app_config_load_defaults();

	if (load_record_if_valid(flash_record) != 0U)
	{
		return;
	}

	if (is_flash_region_erased((const uint8_t *)flash_record, sizeof(*flash_record)) != 0U)
	{
		return;
	}
}

/**
 * @brief Return the active in-memory product name.
 * @return Null-terminated product name.
 */
const char *app_config_get_product_name(void)
{
	return current_app_config.product_name;
}

/**
 * @brief Update the volatile product name used by the runtime and next save.
 * @param product_name Null-terminated ASCII product name.
 * @return Operation status.
 */
app_config_status_t app_config_set_product_name(const char *product_name)
{
	app_config_status_t status = validate_product_name(product_name);

	if (status != APP_CONFIG_STATUS_OK)
	{
		return status;
	}

	(void)strncpy(current_app_config.product_name, product_name, sizeof(current_app_config.product_name) - 1U);
	current_app_config.product_name[sizeof(current_app_config.product_name) - 1U] = '\0';
	return APP_CONFIG_STATUS_OK;
}

/**
 * @brief Erase the reserved flash sector and program the current config record.
 * @return Operation status.
 */
app_config_status_t app_config_save(void)
{
	persisted_app_config_t record;
	FLASH_EraseInitTypeDef erase_init = {0};
	uint32_t sector_error = 0U;
	uint32_t offset;

	build_persisted_record(&record);
	last_flash_error = HAL_FLASH_ERROR_NONE;
	last_sector_error = 0xFFFFFFFFUL;

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1 | FLASH_FLAG_ALL_ERRORS_BANK2);

	erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase_init.Banks = APP_CONFIG_FLASH_BANK;
	erase_init.Sector = APP_CONFIG_FLASH_SECTOR;
	erase_init.NbSectors = 1U;
	erase_init.VoltageRange = APP_CONFIG_FLASH_VOLTAGE_RANGE;

	if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK)
	{
		last_flash_error = HAL_FLASH_GetError();
		last_sector_error = sector_error;
		(void)HAL_FLASH_Lock();
		return APP_CONFIG_STATUS_FLASH_ERROR;
	}

	for (offset = 0U; offset < sizeof(record); offset += APP_CONFIG_FLASHWORD_SIZE)
	{
		if (HAL_FLASH_Program(
				FLASH_TYPEPROGRAM_FLASHWORD,
				APP_CONFIG_FLASH_ADDRESS + offset,
				(uint32_t)((uintptr_t)&((const uint8_t *)&record)[offset])) != HAL_OK)
		{
			last_flash_error = HAL_FLASH_GetError();
			(void)HAL_FLASH_Lock();
			return APP_CONFIG_STATUS_FLASH_ERROR;
		}
	}

	(void)HAL_FLASH_Lock();

	return APP_CONFIG_STATUS_OK;
}

/**
 * @brief Read the most recent HAL flash error bitmap captured during save.
 * @return HAL flash error bitmap.
 */
uint32_t app_config_get_last_flash_error(void)
{
	return last_flash_error;
}

/**
 * @brief Read the most recent sector index returned by a failed erase.
 * @return Sector index reported by HAL, or `0xFFFFFFFF` when not applicable.
 */
uint32_t app_config_get_last_sector_error(void)
{
	return last_sector_error;
}

/**
 * @brief Restore the built-in configuration values used on virgin or invalid flash.
 */
static void app_config_load_defaults(void)
{
	(void)strncpy(
			current_app_config.product_name,
			APP_CONFIG_DEFAULT_PRODUCT_NAME,
			sizeof(current_app_config.product_name) - 1U);
	current_app_config.product_name[sizeof(current_app_config.product_name) - 1U] = '\0';
}

/**
 * @brief Reject product names that would break persistence or the UART protocol.
 * @param product_name Null-terminated candidate product name.
 * @return Validation status.
 */
static app_config_status_t validate_product_name(const char *product_name)
{
	size_t index;
	size_t length;

	if (product_name == NULL)
	{
		return APP_CONFIG_STATUS_INVALID_ARGUMENT;
	}

	length = strlen(product_name);
	if ((length == 0U) || (length > APP_CONFIG_PRODUCT_NAME_MAX_LEN))
	{
		return APP_CONFIG_STATUS_INVALID_ARGUMENT;
	}

	for (index = 0U; index < length; index++)
	{
		uint8_t byte = (uint8_t)product_name[index];

		if ((byte < 0x20U) || (byte > 0x7EU) || (byte == ','))
		{
			return APP_CONFIG_STATUS_INVALID_ARGUMENT;
		}
	}

	return APP_CONFIG_STATUS_OK;
}

/**
 * @brief Calculate the record CRC32 using the Ethernet polynomial.
 * @param data Bytes to hash.
 * @param length Number of bytes in @p data.
 * @return CRC32 value.
 */
static uint32_t calculate_crc32(const uint8_t *data, size_t length)
{
	uint32_t crc = 0xFFFFFFFFUL;
	size_t index;

	for (index = 0U; index < length; index++)
	{
		crc ^= data[index];
		for (uint32_t bit = 0U; bit < 8U; bit++)
		{
			if ((crc & 1UL) != 0UL)
			{
				crc = (crc >> 1U) ^ 0xEDB88320UL;
			}
			else
			{
				crc >>= 1U;
			}
		}
	}

	return ~crc;
}

/**
 * @brief Detect whether the reserved config region still contains erased flash.
 * @param data Address of the flash region to inspect.
 * @param length Number of bytes to inspect.
 * @return Non-zero when every byte is erased, otherwise zero.
 */
static uint32_t is_flash_region_erased(const uint8_t *data, size_t length)
{
	for (size_t index = 0U; index < length; index++)
	{
		if (data[index] != 0xFFU)
		{
			return 0U;
		}
	}

	return 1U;
}

/**
 * @brief Build the flash image for the current runtime configuration.
 * @param record Destination record that will be programmed to flash.
 */
static void build_persisted_record(persisted_app_config_t *record)
{
	(void)memset(record, 0xFF, sizeof(*record));
	record->magic = APP_CONFIG_MAGIC;
	record->version = APP_CONFIG_VERSION;
	record->payload_size = sizeof(record->product_name);
	record->reserved0 = 0U;
	(void)memcpy(record->product_name, current_app_config.product_name, sizeof(record->product_name));
	record->crc32 = calculate_crc32((const uint8_t *)record, offsetof(persisted_app_config_t, crc32));
	for (uint32_t index = 0U; index < 3U; index++)
	{
		record->reserved1[index] = 0xFFFFFFFFUL;
	}
}

/**
 * @brief Validate a persisted flash record and load it into the runtime config.
 * @param record Persisted flash record mapped from internal flash.
 * @return Non-zero when the record was accepted and loaded, otherwise zero.
 */
static uint32_t load_record_if_valid(const persisted_app_config_t *record)
{
	uint32_t expected_crc;

	if ((record->magic != APP_CONFIG_MAGIC) ||
			(record->version != APP_CONFIG_VERSION) ||
			(record->payload_size != sizeof(record->product_name)))
	{
		return 0U;
	}

	expected_crc = calculate_crc32((const uint8_t *)record, offsetof(persisted_app_config_t, crc32));
	if (record->crc32 != expected_crc)
	{
		return 0U;
	}

	if (validate_product_name(record->product_name) != APP_CONFIG_STATUS_OK)
	{
		return 0U;
	}

	(void)memcpy(current_app_config.product_name, record->product_name, sizeof(current_app_config.product_name));
	current_app_config.product_name[sizeof(current_app_config.product_name) - 1U] = '\0';
	return 1U;
}