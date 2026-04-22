#ifndef FLASH_CONFIG_STORAGE_BSP_H
#define FLASH_CONFIG_STORAGE_BSP_H

/**
 * @file flash_config_storage_bsp.h
 * @brief Board-support flash persistence for configuration data.
 * @ingroup config_services
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Maximum stored product-name length excluding the terminating null character. */
#define FLASH_CONFIG_STORAGE_PRODUCT_NAME_MAX_LEN    31U

/** Result codes returned by the flash-backed configuration storage layer. */
typedef enum
{
	/** Operation completed successfully. */
	FLASH_CONFIG_STORAGE_STATUS_OK = 0,
	/** Caller provided invalid input. */
	FLASH_CONFIG_STORAGE_STATUS_INVALID_ARGUMENT = 1,
	/** No valid configuration record was found in flash. */
	FLASH_CONFIG_STORAGE_STATUS_NOT_FOUND = 2,
	/** Flash contained a record but its contents failed validation. */
	FLASH_CONFIG_STORAGE_STATUS_INVALID_DATA = 3,
	/** Underlying flash driver reported an error. */
	FLASH_CONFIG_STORAGE_STATUS_FLASH_ERROR = 4,
} flash_config_storage_bsp_status_t;

/** Persistent configuration payload stored in flash. */
typedef struct
{
	/** Null-terminated product name persisted across reset. */
	char product_name[FLASH_CONFIG_STORAGE_PRODUCT_NAME_MAX_LEN + 1U];
} flash_config_storage_data_t;

/**
 * @brief Load configuration data from the reserved flash sector.
 * @param config_data Destination for the loaded configuration.
 * @return Storage-layer status.
 */
flash_config_storage_bsp_status_t flash_config_storage_bsp_load(flash_config_storage_data_t *config_data);

/**
 * @brief Save configuration data into the reserved flash sector.
 * @param config_data Configuration payload to persist.
 * @return Storage-layer status.
 */
flash_config_storage_bsp_status_t flash_config_storage_bsp_save(const flash_config_storage_data_t *config_data);

/**
 * @brief Read the last HAL flash error code seen by the storage layer.
 * @return HAL flash error bitmap.
 */
uint32_t flash_config_storage_bsp_get_last_flash_error(void);

/**
 * @brief Read the last failing sector code seen by the storage layer.
 * @return HAL sector-error identifier.
 */
uint32_t flash_config_storage_bsp_get_last_sector_error(void);

#ifdef __cplusplus
}
#endif

#endif