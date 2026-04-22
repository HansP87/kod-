#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "flash_config_storage_bsp.h"
#include "main.h"
#include "stm32h7xx_hal_flash_ex.h"

#define FLASH_CONFIG_STORAGE_ADDRESS          0x081FC000UL
#define FLASH_CONFIG_STORAGE_SECTOR           FLASH_SECTOR_63
#define FLASH_CONFIG_STORAGE_BANK             FLASH_BANK_2
#define FLASH_CONFIG_STORAGE_VOLTAGE_RANGE    0U
#define FLASH_CONFIG_STORAGE_MAGIC            0x43464731UL
#define FLASH_CONFIG_STORAGE_VERSION          1UL
#define FLASH_CONFIG_STORAGE_FLASHWORD_SIZE   16U

typedef struct __attribute__((aligned(FLASH_CONFIG_STORAGE_FLASHWORD_SIZE)))
{
	uint32_t magic;
	uint32_t version;
	uint32_t payload_size;
	uint32_t reserved0;
	char product_name[FLASH_CONFIG_STORAGE_PRODUCT_NAME_MAX_LEN + 1U];
	uint32_t crc32;
	uint32_t reserved1[3];
} flash_config_record_t;

static uint32_t last_flash_error = 0U;
static uint32_t last_sector_error = 0U;

static uint32_t flash_config_storage_calculate_crc32(const uint8_t *data, size_t length);
static uint32_t flash_config_storage_is_region_erased(const uint8_t *data, size_t length);
static void build_flash_config_record(flash_config_record_t *record, const flash_config_storage_data_t *config_data);
static flash_config_storage_bsp_status_t try_load_flash_config_record(
		const flash_config_record_t *record,
		flash_config_storage_data_t *config_data);

flash_config_storage_bsp_status_t flash_config_storage_bsp_load(flash_config_storage_data_t *config_data)
{
	const flash_config_record_t *flash_record = (const flash_config_record_t *)FLASH_CONFIG_STORAGE_ADDRESS;

	if (config_data == NULL)
	{
		return FLASH_CONFIG_STORAGE_STATUS_INVALID_ARGUMENT;
	}

	if (try_load_flash_config_record(flash_record, config_data) == FLASH_CONFIG_STORAGE_STATUS_OK)
	{
		return FLASH_CONFIG_STORAGE_STATUS_OK;
	}

	if (flash_config_storage_is_region_erased((const uint8_t *)flash_record, sizeof(*flash_record)) != 0U)
	{
		return FLASH_CONFIG_STORAGE_STATUS_NOT_FOUND;
	}

	return FLASH_CONFIG_STORAGE_STATUS_INVALID_DATA;
}

flash_config_storage_bsp_status_t flash_config_storage_bsp_save(const flash_config_storage_data_t *config_data)
{
	flash_config_record_t record;
	FLASH_EraseInitTypeDef erase_init = {0};
	uint32_t sector_error = 0U;
	uint32_t offset;

	if (config_data == NULL)
	{
		return FLASH_CONFIG_STORAGE_STATUS_INVALID_ARGUMENT;
	}

	build_flash_config_record(&record, config_data);
	last_flash_error = HAL_FLASH_ERROR_NONE;
	last_sector_error = 0xFFFFFFFFUL;

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1 | FLASH_FLAG_ALL_ERRORS_BANK2);

	erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase_init.Banks = FLASH_CONFIG_STORAGE_BANK;
	erase_init.Sector = FLASH_CONFIG_STORAGE_SECTOR;
	erase_init.NbSectors = 1U;
	erase_init.VoltageRange = FLASH_CONFIG_STORAGE_VOLTAGE_RANGE;

	if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK)
	{
		last_flash_error = HAL_FLASH_GetError();
		last_sector_error = sector_error;
		(void)HAL_FLASH_Lock();
		return FLASH_CONFIG_STORAGE_STATUS_FLASH_ERROR;
	}

	for (offset = 0U; offset < sizeof(record); offset += FLASH_CONFIG_STORAGE_FLASHWORD_SIZE)
	{
		if (HAL_FLASH_Program(
				FLASH_TYPEPROGRAM_FLASHWORD,
				FLASH_CONFIG_STORAGE_ADDRESS + offset,
				(uint32_t)((uintptr_t)&((const uint8_t *)&record)[offset])) != HAL_OK)
		{
			last_flash_error = HAL_FLASH_GetError();
			(void)HAL_FLASH_Lock();
			return FLASH_CONFIG_STORAGE_STATUS_FLASH_ERROR;
		}
	}

	(void)HAL_FLASH_Lock();
	return FLASH_CONFIG_STORAGE_STATUS_OK;
}

uint32_t flash_config_storage_bsp_get_last_flash_error(void)
{
	return last_flash_error;
}

uint32_t flash_config_storage_bsp_get_last_sector_error(void)
{
	return last_sector_error;
}

static uint32_t flash_config_storage_calculate_crc32(const uint8_t *data, size_t length)
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

static uint32_t flash_config_storage_is_region_erased(const uint8_t *data, size_t length)
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

static void build_flash_config_record(flash_config_record_t *record, const flash_config_storage_data_t *config_data)
{
	(void)memset(record, 0xFF, sizeof(*record));
	record->magic = FLASH_CONFIG_STORAGE_MAGIC;
	record->version = FLASH_CONFIG_STORAGE_VERSION;
	record->payload_size = sizeof(record->product_name);
	record->reserved0 = 0U;
	(void)memcpy(record->product_name, config_data->product_name, sizeof(record->product_name));
	record->crc32 = flash_config_storage_calculate_crc32((const uint8_t *)record, offsetof(flash_config_record_t, crc32));
	for (uint32_t index = 0U; index < 3U; index++)
	{
		record->reserved1[index] = 0xFFFFFFFFUL;
	}
}

static flash_config_storage_bsp_status_t try_load_flash_config_record(
		const flash_config_record_t *record,
		flash_config_storage_data_t *config_data)
{
	uint32_t expected_crc;

	if ((record->magic != FLASH_CONFIG_STORAGE_MAGIC) ||
			(record->version != FLASH_CONFIG_STORAGE_VERSION) ||
			(record->payload_size != sizeof(record->product_name)))
	{
		return FLASH_CONFIG_STORAGE_STATUS_INVALID_DATA;
	}

	expected_crc = flash_config_storage_calculate_crc32((const uint8_t *)record, offsetof(flash_config_record_t, crc32));
	if (record->crc32 != expected_crc)
	{
		return FLASH_CONFIG_STORAGE_STATUS_INVALID_DATA;
	}

	(void)memcpy(config_data->product_name, record->product_name, sizeof(config_data->product_name));
	config_data->product_name[sizeof(config_data->product_name) - 1U] = '\0';
	return FLASH_CONFIG_STORAGE_STATUS_OK;
}