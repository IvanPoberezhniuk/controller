#include "encoder.h"
#include "motor_control.h"
#include "motor_math.h"
#include "configuration.h"
#include "board.h"

typedef struct {
    TIM_HandleTypeDef *timer;
    unsigned bits;
} encoder_hw_t;

static const encoder_hw_t s_hw[UGV_MOTOR_COUNT] = {
    [MOTOR_FRONT]  = { &htim2, 32u },
    [MOTOR_CENTER] = { &htim3, 16u },
    [MOTOR_REAR]   = { &htim4, 16u },
};

/* Consecutive zero-delta ticks while the motor is actively commanded before
 * the encoder is considered disconnected/frozen, per
 * ugv-motor-driver-encoders SKILL.md. 250 ticks at the 500 Hz control rate
 * is 500 ms. */
#define ENCODER_STALL_TICKS 250u

typedef struct {
    uint32_t last_count;
    uint32_t frozen_ticks;
} encoder_internal_t;

static encoder_internal_t s_internal[UGV_MOTOR_COUNT];

void encoder_init(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        HAL_TIM_Encoder_Start(s_hw[m].timer, TIM_CHANNEL_ALL);
        s_internal[m].last_count = __HAL_TIM_GET_COUNTER(s_hw[m].timer);
        s_internal[m].frozen_ticks = 0u;
    }
}

void encoder_update(float dt_s)
{
    const ugv_config_t *cfg = config_get();
    float counts_per_rev = (float)cfg->encoder_counts_per_output_rev;

    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        encoder_internal_t *in = &s_internal[m];
        uint32_t count = __HAL_TIM_GET_COUNTER(s_hw[m].timer);
        int32_t delta = mm_encoder_delta(in->last_count, count, s_hw[m].bits);
        in->last_count = count;

        MotorState *st = motor_control_get_state_mutable(m);
        st->encoder_count += delta;
        st->encoder_delta = delta;

        float rpm = 0.0f;
        if (counts_per_rev > 0.0f && dt_s > 0.0f) {
            rpm = ((float)delta / counts_per_rev) * (60.0f / dt_s);
        }
        st->measured_rpm = rpm;

        bool commanded = st->enabled &&
            ((st->pwm_command > cfg->pwm_dead_zone) || (st->pwm_command < -cfg->pwm_dead_zone));

        if (commanded && delta == 0) {
            if (in->frozen_ticks < ENCODER_STALL_TICKS) {
                in->frozen_ticks++;
            }
        } else {
            in->frozen_ticks = 0u;
        }

        st->encoder_valid = (in->frozen_ticks < ENCODER_STALL_TICKS);
    }
}
