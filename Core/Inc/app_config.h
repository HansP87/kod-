#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Maximum length of the product name excluding the terminating null byte. */
#define APP_CONFIG_PRODUCT_NAME_MAX_LEN    31U

/** Result codes returned by configuration update and save APIs. */
typedef enum
{
	APP_CONFIG_STATUS_OK = 0,
	APP_CONFIG_STATUS_INVALID_ARGUMENT = 1,
	APP_CONFIG_STATUS_FLASH_ERROR = 2,
	APP_CONFIG_STATUS_VERIFY_ERROR = 3,
} app_config_status_t;

/**
 * @brief Load the persisted configuration from flash or fall back to defaults.
 */
void app_config_init(void);

/**
 * @brief Get the current in-memory product name.
 * @return Null-terminated product name string.
 */
const char *app_config_get_product_name(void);

/**
 * @brief Update the in-memory product name without persisting it.
 * @param product_name Null-terminated ASCII product name.
 * @return Operation status.
 */
app_config_status_t app_config_set_product_name(const char *product_name);

/**
 * @brief Persist the current in-memory configuration to flash.
 * @return Operation status.
 */
app_config_status_t app_config_save(void);

/**
 * @brief Read the most recent HAL flash error bits captured by the config module.
 * @return Last HAL flash error bitmap.
 */
uint32_t app_config_get_last_flash_error(void);

/**
 * @brief Read the most recent sector index reported on erase failure.
 * @return Last sector error value returned by HAL.
 */
uint32_t app_config_get_last_sector_error(void);

#ifdef __cplusplus
}
#endif

#endif