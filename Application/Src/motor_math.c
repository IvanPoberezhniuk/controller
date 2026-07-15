#include "motor_math.h"

float mm_ramp_toward(float current, float target, float max_delta)
{
    float delta = target - current;
    if (max_delta <= 0.0f) {
        return target;
    }
    if (delta > max_delta) {
        return current + max_delta;
    }
    if (delta < -max_delta) {
        return current - max_delta;
    }
    return target;
}

float mm_pid_step(float target, float measured, float dt_s,
                   float kp, float ki, float kd,
                   float *integral, float *previous_error,
                   float integral_clamp)
{
    float error = target - measured;

    *integral += error * dt_s;
    if (*integral > integral_clamp) {
        *integral = integral_clamp;
    } else if (*integral < -integral_clamp) {
        *integral = -integral_clamp;
    }

    float derivative = (dt_s > 0.0f) ? (error - *previous_error) / dt_s : 0.0f;
    *previous_error = error;

    return (kp * error) + (ki * (*integral)) + (kd * derivative);
}

int32_t mm_encoder_delta(uint32_t previous_count, uint32_t current_count, unsigned counter_bits)
{
    uint32_t mask = (counter_bits >= 32u) ? 0xFFFFFFFFu : ((1u << counter_bits) - 1u);
    uint32_t raw = (current_count - previous_count) & mask;
    uint32_t half = (mask / 2u) + 1u;

    if (raw >= half) {
        return (int32_t)(raw - mask - 1u);
    }
    return (int32_t)raw;
}

int mm_sign(float value)
{
    if (value > 0.0f) {
        return 1;
    }
    if (value < 0.0f) {
        return -1;
    }
    return 0;
}
