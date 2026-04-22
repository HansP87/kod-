#include "app_mode_service.h"

static uint32_t app_mode_state = APP_MODE_STREAMING;
static uint32_t button_event_timestamp_us = 0U;

void app_mode_service_set_mode(app_mode_t mode)
{
	__atomic_store_n(&app_mode_state, (uint32_t)mode, __ATOMIC_RELEASE);
}

app_mode_t app_mode_service_get_mode(void)
{
	return (app_mode_t)__atomic_load_n(&app_mode_state, __ATOMIC_ACQUIRE);
}

uint32_t app_mode_service_is_streaming_mode(void)
{
	return (uint32_t)(app_mode_service_get_mode() == APP_MODE_STREAMING);
}

void app_mode_service_set_button_event_timestamp_us(uint32_t timestamp_us)
{
	__atomic_store_n(&button_event_timestamp_us, timestamp_us, __ATOMIC_RELEASE);
}

uint32_t app_mode_service_get_button_event_timestamp_us(void)
{
	return __atomic_load_n(&button_event_timestamp_us, __ATOMIC_ACQUIRE);
}