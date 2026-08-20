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

/* Sign applied to each motor's encoder delta so that positive drive
 * (RPWM active) reads as positive measured_rpm -- with the sign wrong the
 * PID sees inverted feedback and rails PWM to the cap instead of
 * regulating. Motor0's A/B wiring counts down when driving forward, so it
 * is flipped here rather than re-pinning the connector; verify motors 1/2
 * the same way during their bring-up. */
static const int32_t s_count_sign[UGV_MOTOR_COUNT] = {
    [MOTOR_FRONT]  = -1,
    [MOTOR_CENTER] = 1,
    [MOTOR_REAR]   = 1,
};

/* Consecutive zero-delta ticks while the motor is actively commanded before
 * the encoder is considered disconnected/frozen, per
 * ugv-motor-driver-encoders SKILL.md. 250 ticks at the 500 Hz control rate
 * is 500 ms. */
#define ENCODER_STALL_TICKS 250u

/* Speed is computed from the summed delta over this many control ticks, not
 * from a single tick: at 500 Hz one tick spans only 2 ms, which with 2640
 * counts/rev quantizes instantaneous RPM to steps of 60/(2640*0.002) = 11.4
 * RPM -- worse than the whole low-speed operating range, and the PID chases
 * that noise into oscillation. 25 ticks = 50 ms window = 0.45 RPM
 * resolution, still fast against the mechanical time constant. */
#define SPEED_WINDOW_TICKS 25u

typedef struct {
    uint32_t last_count;
    uint32_t frozen_ticks;
    bool     fault_latched;
    int32_t  delta_window[SPEED_WINDOW_TICKS];
    uint32_t window_index;
    int32_t  window_sum;
} encoder_internal_t;

static encoder_internal_t s_internal[UGV_MOTOR_COUNT];

void encoder_init(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        HAL_TIM_Encoder_Start(s_hw[m].timer, TIM_CHANNEL_ALL);
        s_internal[m].last_count = __HAL_TIM_GET_COUNTER(s_hw[m].timer);
        s_internal[m].frozen_ticks = 0u;
        s_internal[m].fault_latched = false;
    }
}

void encoder_update(float dt_s)
{
    const ugv_config_t *cfg = config_get();
    float counts_per_rev = (float)cfg->encoder_counts_per_output_rev;

    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        encoder_internal_t *in = &s_internal[m];
        uint32_t count = __HAL_TIM_GET_COUNTER(s_hw[m].timer);
        int32_t delta = s_count_sign[m] *
            mm_encoder_delta(in->last_count, count, s_hw[m].bits);
        in->last_count = count;

        MotorState *st = motor_control_get_state_mutable(m);
        st->encoder_count += delta;
        st->encoder_delta = delta;

        in->window_sum -= in->delta_window[in->window_index];
        in->delta_window[in->window_index] = delta;
        in->window_sum += delta;
        in->window_index = (in->window_index + 1u) % SPEED_WINDOW_TICKS;

        float rpm = 0.0f;
        if (counts_per_rev > 0.0f && dt_s > 0.0f) {
            rpm = ((float)in->window_sum / counts_per_rev) *
                  (60.0f / (dt_s * (float)SPEED_WINDOW_TICKS));
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

        if (in->frozen_ticks >= ENCODER_STALL_TICKS) {
            in->fault_latched = true;
        }
        st->encoder_valid = !in->fault_latched;
    }
}

void encoder_clear_latched_faults(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        s_internal[m].frozen_ticks = 0u;
        s_internal[m].fault_latched = false;
        motor_control_get_state_mutable(m)->encoder_valid = true;
    }
}
