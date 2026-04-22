/**
 * @file imu_coning_sculling_bortz3.c
 * @brief Standalone future-work prototype for 3.2 kHz to 100 Hz coning/sculling compensation.
 *
 * This file is intentionally not integrated into the current firmware build.
 *
 * Assumptions for this prototype:
 * - Gyro inputs are body-frame angular rates in rad/s at 3.2 kHz.
 * - Accelerometer inputs are body-frame specific force samples in m/s^2 at 3.2 kHz.
 * - A causal cubic Lagrange fractional-delay filter can be applied on the
 *   accelerometer path to rephase acceleration against the gyro samples.
 * - One compensated output is emitted every 32 input samples, i.e. at 100 Hz.
 * - The correction stencil uses third-order Bortz-style coning coefficients over
 *   consecutive 3-sample windows inside each 10 ms major interval.
 */

#include "imu_coning_sculling_bortz3.h"

#include <stddef.h>
#include <string.h>

#define IMU_BORTZ3_CONING_NEAR_GAIN     (33.0f / 80.0f)
#define IMU_BORTZ3_CONING_FAR_GAIN      (57.0f / 80.0f)
#define IMU_BORTZ3_SCULLING_NEAR_GAIN   (33.0f / 80.0f)
#define IMU_BORTZ3_SCULLING_FAR_GAIN    (57.0f / 80.0f)
#define IMU_BORTZ3_REPHASE_MAX_DELAY    3.0f

static void imu_bortz3_reset_interval(imu_bortz3_state_t *state);
static float imu_bortz3_clamp_float(float value, float min_value, float max_value);
static imu_bortz3_vec3_t imu_bortz3_vec3_add(imu_bortz3_vec3_t lhs, imu_bortz3_vec3_t rhs);
static imu_bortz3_vec3_t imu_bortz3_vec3_scale(imu_bortz3_vec3_t vector, float scale);
static imu_bortz3_vec3_t imu_bortz3_vec3_cross(imu_bortz3_vec3_t lhs, imu_bortz3_vec3_t rhs);
static void imu_bortz3_push_accel_rephase_sample(
	imu_bortz3_state_t *state,
	imu_bortz3_vec3_t accel_sample);
static imu_bortz3_vec3_t imu_bortz3_apply_accel_rephase(
	const imu_bortz3_state_t *state,
	imu_bortz3_vec3_t accel_sample);
static float imu_bortz3_lagrange_coefficient(float delay_samples, uint32_t tap_index);
static imu_bortz3_vec3_t imu_bortz3_compute_coning_triplet(
	imu_bortz3_vec3_t first,
	imu_bortz3_vec3_t second,
	imu_bortz3_vec3_t third);
static imu_bortz3_vec3_t imu_bortz3_compute_sculling_triplet(
	imu_bortz3_vec3_t first_delta_angle,
	imu_bortz3_vec3_t first_delta_velocity,
	imu_bortz3_vec3_t second_delta_angle,
	imu_bortz3_vec3_t second_delta_velocity,
	imu_bortz3_vec3_t third_delta_angle,
	imu_bortz3_vec3_t third_delta_velocity);

void imu_bortz3_init(imu_bortz3_state_t *state)
{
	if (state == NULL)
	{
		return;
	}

	state->accel_rephase_delay_samples = 0.0f;
	imu_bortz3_reset_interval(state);
}

void imu_bortz3_set_accel_rephase_delay_samples(
	imu_bortz3_state_t *state,
	float delay_samples)
{
	if (state == NULL)
	{
		return;
	}

	state->accel_rephase_delay_samples = imu_bortz3_clamp_float(
		delay_samples,
		0.0f,
		IMU_BORTZ3_REPHASE_MAX_DELAY);

	memset(state->accel_rephase_history, 0, sizeof(state->accel_rephase_history));
	state->accel_rephase_history_count = 0U;
}

bool imu_bortz3_push_sample(
	imu_bortz3_state_t *state,
	const imu_bortz3_input_sample_t *input_sample,
	imu_bortz3_output_t *output)
{
	imu_bortz3_vec3_t delta_angle;
	imu_bortz3_vec3_t rephased_accel;
	imu_bortz3_vec3_t delta_velocity;

	if ((state == NULL) || (input_sample == NULL) || (output == NULL))
	{
		return false;
	}

	imu_bortz3_push_accel_rephase_sample(state, input_sample->accel_mps2);
	rephased_accel = imu_bortz3_apply_accel_rephase(state, input_sample->accel_mps2);

	delta_angle = imu_bortz3_vec3_scale(input_sample->gyro_rad_s, IMU_BORTZ3_INPUT_DT_S);
	delta_velocity = imu_bortz3_vec3_scale(rephased_accel, IMU_BORTZ3_INPUT_DT_S);

	state->accumulated_delta_angle = imu_bortz3_vec3_add(state->accumulated_delta_angle, delta_angle);
	state->accumulated_delta_velocity = imu_bortz3_vec3_add(state->accumulated_delta_velocity, delta_velocity);

	if (state->sample_count >= 2U)
	{
		state->accumulated_coning_correction = imu_bortz3_vec3_add(
			state->accumulated_coning_correction,
			imu_bortz3_compute_coning_triplet(
				state->previous_delta_angle[0],
				state->previous_delta_angle[1],
				delta_angle));

		state->accumulated_sculling_correction = imu_bortz3_vec3_add(
			state->accumulated_sculling_correction,
			imu_bortz3_compute_sculling_triplet(
				state->previous_delta_angle[0],
				state->previous_delta_velocity[0],
				state->previous_delta_angle[1],
				state->previous_delta_velocity[1],
				delta_angle,
				delta_velocity));
	}

	state->previous_delta_angle[0] = state->previous_delta_angle[1];
	state->previous_delta_angle[1] = delta_angle;
	state->previous_delta_velocity[0] = state->previous_delta_velocity[1];
	state->previous_delta_velocity[1] = delta_velocity;
	state->sample_count++;

	if (state->sample_count < IMU_BORTZ3_SAMPLES_PER_OUTPUT)
	{
		return false;
	}

	output->delta_angle_rad = imu_bortz3_vec3_add(
		state->accumulated_delta_angle,
		state->accumulated_coning_correction);

	output->delta_velocity_mps = imu_bortz3_vec3_add(
		imu_bortz3_vec3_add(
			state->accumulated_delta_velocity,
			imu_bortz3_vec3_scale(
				imu_bortz3_vec3_cross(
					state->accumulated_delta_angle,
					state->accumulated_delta_velocity),
				0.5f)),
		state->accumulated_sculling_correction);

	output->interval_s = IMU_BORTZ3_OUTPUT_DT_S;
	output->sample_count = IMU_BORTZ3_SAMPLES_PER_OUTPUT;

	imu_bortz3_reset_interval(state);
	return true;
}

static void imu_bortz3_reset_interval(imu_bortz3_state_t *state)
{
	if (state == NULL)
	{
		return;
	}

	state->sample_count = 0U;
	state->accumulated_delta_angle.x = 0.0f;
	state->accumulated_delta_angle.y = 0.0f;
	state->accumulated_delta_angle.z = 0.0f;
	state->accumulated_delta_velocity.x = 0.0f;
	state->accumulated_delta_velocity.y = 0.0f;
	state->accumulated_delta_velocity.z = 0.0f;
	state->accumulated_coning_correction.x = 0.0f;
	state->accumulated_coning_correction.y = 0.0f;
	state->accumulated_coning_correction.z = 0.0f;
	state->accumulated_sculling_correction.x = 0.0f;
	state->accumulated_sculling_correction.y = 0.0f;
	state->accumulated_sculling_correction.z = 0.0f;
	memset(state->previous_delta_angle, 0, sizeof(state->previous_delta_angle));
	memset(state->previous_delta_velocity, 0, sizeof(state->previous_delta_velocity));
	memset(state->accel_rephase_history, 0, sizeof(state->accel_rephase_history));
	state->accel_rephase_history_count = 0U;
}

static float imu_bortz3_clamp_float(float value, float min_value, float max_value)
{
	if (value < min_value)
	{
		return min_value;
	}

	if (value > max_value)
	{
		return max_value;
	}

	return value;
}

static imu_bortz3_vec3_t imu_bortz3_vec3_add(imu_bortz3_vec3_t lhs, imu_bortz3_vec3_t rhs)
{
	imu_bortz3_vec3_t result;

	result.x = lhs.x + rhs.x;
	result.y = lhs.y + rhs.y;
	result.z = lhs.z + rhs.z;
	return result;
}

static imu_bortz3_vec3_t imu_bortz3_vec3_scale(imu_bortz3_vec3_t vector, float scale)
{
	imu_bortz3_vec3_t result;

	result.x = vector.x * scale;
	result.y = vector.y * scale;
	result.z = vector.z * scale;
	return result;
}

static imu_bortz3_vec3_t imu_bortz3_vec3_cross(imu_bortz3_vec3_t lhs, imu_bortz3_vec3_t rhs)
{
	imu_bortz3_vec3_t result;

	result.x = (lhs.y * rhs.z) - (lhs.z * rhs.y);
	result.y = (lhs.z * rhs.x) - (lhs.x * rhs.z);
	result.z = (lhs.x * rhs.y) - (lhs.y * rhs.x);
	return result;
}

static void imu_bortz3_push_accel_rephase_sample(
	imu_bortz3_state_t *state,
	imu_bortz3_vec3_t accel_sample)
{
	uint32_t index;

	for (index = IMU_BORTZ3_REPHASE_HISTORY_LEN - 1U; index > 0U; index--)
	{
		state->accel_rephase_history[index] = state->accel_rephase_history[index - 1U];
	}

	state->accel_rephase_history[0] = accel_sample;

	if (state->accel_rephase_history_count < IMU_BORTZ3_REPHASE_HISTORY_LEN)
	{
		state->accel_rephase_history_count++;
	}
}

static imu_bortz3_vec3_t imu_bortz3_apply_accel_rephase(
	const imu_bortz3_state_t *state,
	imu_bortz3_vec3_t accel_sample)
{
	imu_bortz3_vec3_t result;
	uint32_t tap_index;

	if ((state->accel_rephase_delay_samples <= 0.0f)
		|| (state->accel_rephase_history_count < IMU_BORTZ3_REPHASE_HISTORY_LEN))
	{
		return accel_sample;
	}

	result.x = 0.0f;
	result.y = 0.0f;
	result.z = 0.0f;

	for (tap_index = 0U; tap_index < IMU_BORTZ3_REPHASE_HISTORY_LEN; tap_index++)
	{
		float coefficient = imu_bortz3_lagrange_coefficient(
			state->accel_rephase_delay_samples,
			tap_index);

		result.x += coefficient * state->accel_rephase_history[tap_index].x;
		result.y += coefficient * state->accel_rephase_history[tap_index].y;
		result.z += coefficient * state->accel_rephase_history[tap_index].z;
	}

	return result;
}

static float imu_bortz3_lagrange_coefficient(float delay_samples, uint32_t tap_index)
{
	switch (tap_index)
	{
		case 0U:
			return -((delay_samples - 1.0f) * (delay_samples - 2.0f) * (delay_samples - 3.0f)) / 6.0f;
		case 1U:
			return (delay_samples * (delay_samples - 2.0f) * (delay_samples - 3.0f)) / 2.0f;
		case 2U:
			return -(delay_samples * (delay_samples - 1.0f) * (delay_samples - 3.0f)) / 2.0f;
		case 3U:
			return (delay_samples * (delay_samples - 1.0f) * (delay_samples - 2.0f)) / 6.0f;
		default:
			return 0.0f;
	}
}

static imu_bortz3_vec3_t imu_bortz3_compute_coning_triplet(
	imu_bortz3_vec3_t first,
	imu_bortz3_vec3_t second,
	imu_bortz3_vec3_t third)
{
	imu_bortz3_vec3_t near_cross_terms;
	imu_bortz3_vec3_t far_cross_term;

	near_cross_terms = imu_bortz3_vec3_add(
		imu_bortz3_vec3_cross(first, second),
		imu_bortz3_vec3_cross(second, third));

	far_cross_term = imu_bortz3_vec3_cross(first, third);

	return imu_bortz3_vec3_add(
		imu_bortz3_vec3_scale(near_cross_terms, IMU_BORTZ3_CONING_NEAR_GAIN),
		imu_bortz3_vec3_scale(far_cross_term, IMU_BORTZ3_CONING_FAR_GAIN));
}

static imu_bortz3_vec3_t imu_bortz3_compute_sculling_triplet(
	imu_bortz3_vec3_t first_delta_angle,
	imu_bortz3_vec3_t first_delta_velocity,
	imu_bortz3_vec3_t second_delta_angle,
	imu_bortz3_vec3_t second_delta_velocity,
	imu_bortz3_vec3_t third_delta_angle,
	imu_bortz3_vec3_t third_delta_velocity)
{
	imu_bortz3_vec3_t near_cross_terms;
	imu_bortz3_vec3_t far_cross_terms;

	near_cross_terms = imu_bortz3_vec3_add(
		imu_bortz3_vec3_add(
			imu_bortz3_vec3_cross(first_delta_angle, second_delta_velocity),
			imu_bortz3_vec3_cross(first_delta_velocity, second_delta_angle)),
		imu_bortz3_vec3_add(
			imu_bortz3_vec3_cross(second_delta_angle, third_delta_velocity),
			imu_bortz3_vec3_cross(second_delta_velocity, third_delta_angle)));

	far_cross_terms = imu_bortz3_vec3_add(
		imu_bortz3_vec3_cross(first_delta_angle, third_delta_velocity),
		imu_bortz3_vec3_cross(first_delta_velocity, third_delta_angle));

	return imu_bortz3_vec3_add(
		imu_bortz3_vec3_scale(near_cross_terms, IMU_BORTZ3_SCULLING_NEAR_GAIN),
		imu_bortz3_vec3_scale(far_cross_terms, IMU_BORTZ3_SCULLING_FAR_GAIN));
}