#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

/**
 * @file config_service.h
 * @brief Runtime configuration accessors backed by persistent flash storage.
 * @ingroup config_services
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum product-name length excluding the terminating null character. */
#define CONFIG_SERVICE_PRODUCT_NAME_MAX_LEN    31U

/** Result codes returned by the configuration service. */
typedef enum
{
	/** Operation completed successfully. */
	CONFIG_SERVICE_STATUS_OK = 0,
	/** Input parameter failed validation. */
	CONFIG_SERVICE_STATUS_INVALID_ARGUMENT = 1,
	/** Flash programming or erase failed. */
	CONFIG_SERVICE_STATUS_FLASH_ERROR = 2,
	/** Data verification after save/load failed. */
	CONFIG_SERVICE_STATUS_VERIFY_ERROR = 3,
} config_service_status_t;

/** @brief Initialize runtime configuration from persistent storage or defaults. */
void config_service_init(void);

/**
 * @brief Read the active product name.
 * @return Null-terminated product name string.
 */
const char *config_service_get_product_name(void);

/**
 * @brief Update the active product name in RAM.
 * @param product_name Null-terminated product name to validate and store.
 * @return Operation status.
 */
config_service_status_t config_service_set_product_name(const char *product_name);

/**
 * @brief Persist the active runtime configuration to flash.
 * @return Operation status.
 */
config_service_status_t config_service_save(void);

#ifdef __cplusplus
}
#endif

#endif