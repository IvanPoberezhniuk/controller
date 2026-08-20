#ifndef APPLICATION_MOTOR_MATH_H
#define APPLICATION_MOTOR_MATH_H

#include <stdbool.h>
#include <stdint.h>

/* Pure math, no HAL/hardware dependency, so Tests/ can exercise it on-host. */

float mm_ramp_toward(float current, float target, float max_delta);

float mm_pid_step(float target, float measured, float dt_s,
                   float kp, float ki, float kd,
                   float *integral, float *previous_error,
                   float integral_clamp);

/* Rollover-safe delta between two encoder readings of the given counter
 * width (16 or 32 bits). */
int32_t mm_encoder_delta(uint32_t previous_count, uint32_t current_count, unsigned counter_bits);

int mm_sign(float value);

bool mm_target_is_valid(float target_rpm, float max_abs_rpm);

#endif /* APPLICATION_MOTOR_MATH_H */
