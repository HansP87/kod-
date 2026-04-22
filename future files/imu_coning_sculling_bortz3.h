#ifndef IMU_CONING_SCULLING_BORTZ3_H
#define IMU_CONING_SCULLING_BORTZ3_H

/**
 * @file imu_coning_sculling_bortz3.h
 * @brief Standalone future-work prototype API for 3.2 kHz to 100 Hz coning/sculling compensation.
 *
 * This header is intentionally not integrated into the current firmware build.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define IMU_BORTZ3_INPUT_RATE_HZ        3200.0f
#define IMU_BORTZ3_OUTPUT_RATE_HZ       100.0f
#define IMU_BORTZ3_SAMPLES_PER_OUTPUT   32U
#define IMU_BORTZ3_INPUT_DT_S           (1.0f / IMU_BORTZ3_INPUT_RATE_HZ)
#define IMU_BORTZ3_OUTPUT_DT_S          (1.0f / IMU_BORTZ3_OUTPUT_RATE_HZ)
#define IMU_BORTZ3_REPHASE_HISTORY_LEN  4U

typedef struct
{
	float x;
	float y;
	float z;
} imu_bortz3_vec3_t;

typedef struct
{
	imu_bortz3_vec3_t gyro_rad_s;
	imu_bortz3_vec3_t accel_mps2;
} imu_bortz3_input_sample_t;

typedef struct
{
	imu_bortz3_vec3_t delta_angle_rad;
	imu_bortz3_vec3_t delta_velocity_mps;
	float interval_s;
	uint32_t sample_count;
} imu_bortz3_output_t;

typedef struct
{
	float accel_rephase_delay_samples;
	uint32_t sample_count;
	imu_bortz3_vec3_t accumulated_delta_angle;
	imu_bortz3_vec3_t accumulated_delta_velocity;
	imu_bortz3_vec3_t accumulated_coning_correction;
	imu_bortz3_vec3_t accumulated_sculling_correction;
	imu_bortz3_vec3_t previous_delta_angle[2];
	imu_bortz3_vec3_t previous_delta_velocity[2];
	imu_bortz3_vec3_t accel_rephase_history[IMU_BORTZ3_REPHASE_HISTORY_LEN];
	uint32_t accel_rephase_history_count;
} imu_bortz3_state_t;

void imu_bortz3_init(imu_bortz3_state_t *state);

void imu_bortz3_set_accel_rephase_delay_samples(
	imu_bortz3_state_t *state,
	float delay_samples);

bool imu_bortz3_push_sample(
	imu_bortz3_state_t *state,
	const imu_bortz3_input_sample_t *input_sample,
	imu_bortz3_output_t *output);

#ifdef __cplusplus
}
#endif

#endif