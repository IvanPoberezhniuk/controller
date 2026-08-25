#include "motor_control.h"
#include "motor_math.h"
#include "configuration.h"
#include "board.h"

typedef struct {
    TIM_HandleTypeDef *rpwm_timer;
    TIM_HandleTypeDef *lpwm_timer;
    uint32_t rpwm_channel;
    uint32_t lpwm_channel;
    GPIO_TypeDef *enable_port;
    uint16_t enable_pin;
} motor_hw_t;

static const motor_hw_t s_hw[UGV_MOTOR_COUNT] = {
    [MOTOR_FRONT] = {
        .rpwm_timer = &htim1, .lpwm_timer = &htim1,
        .rpwm_channel = TIM_CHANNEL_1, .lpwm_channel = TIM_CHANNEL_2,
        .enable_port = MOTOR0_COMMON_EN_GPIO_Port,
        .enable_pin = MOTOR0_COMMON_EN_Pin,
    },
    [MOTOR_CENTER] = {
        .rpwm_timer = &htim1,
#if UGV_FINAL_OTA_PINOUT_CONFIGURED
        .lpwm_timer = &htim16,
        .lpwm_channel = TIM_CHANNEL_1,
#else
        .lpwm_timer = &htim1,
        .lpwm_channel = TIM_CHANNEL_4,
#endif
        .rpwm_channel = TIM_CHANNEL_3,
        .enable_port = MOTOR1_COMMON_EN_GPIO_Port,
        .enable_pin = MOTOR1_COMMON_EN_Pin,
    },
    [MOTOR_REAR] = {
        .rpwm_timer = &htim15, .lpwm_timer = &htim15,
        .rpwm_channel = TIM_CHANNEL_1, .lpwm_channel = TIM_CHANNEL_2,
        .enable_port = MOTOR2_COMMON_EN_GPIO_Port,
        .enable_pin = MOTOR2_COMMON_EN_Pin,
    },
};

static const int8_t s_drive_sign[UGV_MOTOR_COUNT] = {
    [MOTOR_FRONT]  = UGV_MOTOR_FRONT_DIRECTION,
    [MOTOR_CENTER] = UGV_MOTOR_CENTER_DIRECTION,
    [MOTOR_REAR]   = UGV_MOTOR_REAR_DIRECTION,
};

typedef struct {
    float    commanded_rpm;
    int8_t   last_direction; /* -1, 0, +1 */
    uint32_t coast_until_ms; /* 0 = not coasting */
} motor_internal_t;

static MotorState       s_state[UGV_MOTOR_COUNT];
static motor_internal_t s_internal[UGV_MOTOR_COUNT];

static void hw_write_pwm(motor_index_t motor, float pwm_command)
{
    const motor_hw_t *hw = &s_hw[motor];
    float hardware_command = pwm_command * (float)s_drive_sign[motor];
    float magnitude = (hardware_command < 0.0f) ? -hardware_command : hardware_command;
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(hw->rpwm_timer);
    uint32_t compare = (uint32_t)(magnitude * (float)period);

    if (compare > period) {
        compare = period;
    }

    if (hardware_command > 0.0f) {
        __HAL_TIM_SET_COMPARE(hw->rpwm_timer, hw->rpwm_channel, compare);
        __HAL_TIM_SET_COMPARE(hw->lpwm_timer, hw->lpwm_channel, 0u);
    } else if (hardware_command < 0.0f) {
        __HAL_TIM_SET_COMPARE(hw->rpwm_timer, hw->rpwm_channel, 0u);
        __HAL_TIM_SET_COMPARE(hw->lpwm_timer, hw->lpwm_channel, compare);
    } else {
        __HAL_TIM_SET_COMPARE(hw->rpwm_timer, hw->rpwm_channel, 0u);
        __HAL_TIM_SET_COMPARE(hw->lpwm_timer, hw->lpwm_channel, 0u);
    }
}

static void hw_set_driver_enable(motor_index_t motor, bool enabled)
{
    const motor_hw_t *hw = &s_hw[motor];
    GPIO_PinState pin_state = enabled ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(hw->enable_port, hw->enable_pin, pin_state);
}

void motor_control_init(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        const motor_hw_t *hw = &s_hw[m];

        HAL_TIM_PWM_Start(hw->rpwm_timer, hw->rpwm_channel);
        HAL_TIM_PWM_Start(hw->lpwm_timer, hw->lpwm_channel);

        s_state[m] = (MotorState){0};
        s_internal[m] = (motor_internal_t){0};

        hw_write_pwm(m, 0.0f);
        hw_set_driver_enable(m, false);
    }
}

bool motor_control_set_target(motor_index_t motor, float target_rpm)
{
    if ((unsigned)motor >= UGV_MOTOR_COUNT ||
        !mm_target_is_valid(target_rpm, config_get()->max_target_rpm)) {
        return false;
    }
    s_internal[motor].commanded_rpm = target_rpm;
    return true;
}

void motor_control_set_enabled(motor_index_t motor, bool enabled)
{
    s_state[motor].enabled = enabled;
    if (!enabled) {
        s_internal[motor].commanded_rpm = 0.0f;
    }
}

void motor_control_disable_all(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        motor_control_set_enabled(m, false);
        s_state[m].target_rpm = 0.0f;
        s_state[m].pwm_command = 0.0f;
        s_internal[m].last_direction = 0;
        s_internal[m].coast_until_ms = 0u;
        hw_write_pwm(m, 0.0f);
        hw_set_driver_enable(m, false);
    }
}

static void step_one(motor_index_t motor, float dt_s)
{
    const ugv_config_t *cfg = config_get();
    MotorState *st = &s_state[motor];
    motor_internal_t *in = &s_internal[motor];

    if (!st->enabled) {
        st->target_rpm = 0.0f;
        st->pwm_command = 0.0f;
        in->last_direction = 0;
        in->coast_until_ms = 0u;
        hw_write_pwm(motor, 0.0f);
        hw_set_driver_enable(motor, false);
        return;
    }

    hw_set_driver_enable(motor, true);

    uint32_t now = HAL_GetTick();
    if (in->coast_until_ms != 0u) {
        if ((int32_t)(now - in->coast_until_ms) < 0) {
            st->target_rpm = 0.0f;
            st->pwm_command = 0.0f;
            hw_write_pwm(motor, 0.0f);
            return;
        }
        in->coast_until_ms = 0u;
        in->last_direction = mm_sign(in->commanded_rpm);
    }

    int commanded_direction = mm_sign(in->commanded_rpm);
    float ramp_toward_value = in->commanded_rpm;

    if (in->last_direction != 0 && commanded_direction != 0 &&
        commanded_direction != in->last_direction) {
        /* Direction reversal: decelerate to zero, then coast, before the
         * new direction is allowed to start ramping. Do not instantly
         * reverse motor polarity at speed. */
        ramp_toward_value = 0.0f;
    }

    float current_magnitude = (st->target_rpm < 0.0f) ? -st->target_rpm : st->target_rpm;
    float next_magnitude = (ramp_toward_value < 0.0f) ? -ramp_toward_value : ramp_toward_value;
    bool decelerating = next_magnitude < current_magnitude;

    float rate = (decelerating ? cfg->decel_limit_rpm_per_s : cfg->accel_limit_rpm_per_s);
    st->target_rpm = mm_ramp_toward(st->target_rpm, ramp_toward_value, rate * dt_s);

    if (in->last_direction == 0 && commanded_direction != 0) {
        in->last_direction = commanded_direction;
    }

    if (in->last_direction != 0 && commanded_direction != in->last_direction &&
        st->target_rpm == 0.0f && commanded_direction != 0) {
        in->coast_until_ms = now + cfg->direction_change_coast_ms;
        st->pwm_command = 0.0f;
        hw_write_pwm(motor, 0.0f);
        return;
    }

    float pid_out = mm_pid_step(st->target_rpm, st->measured_rpm, dt_s,
                                 cfg->pid_kp, cfg->pid_ki, cfg->pid_kd,
                                 &st->pid_integral, &st->previous_error,
                                 cfg->integral_clamp);
    float pwm_raw = (cfg->feedforward_gain * st->target_rpm) + pid_out;

    if (st->target_rpm != 0.0f && pwm_raw != 0.0f) {
        float magnitude = (pwm_raw < 0.0f) ? -pwm_raw : pwm_raw;
        if (magnitude < cfg->pwm_min_effective) {
            pwm_raw = (pwm_raw < 0.0f) ? -cfg->pwm_min_effective : cfg->pwm_min_effective;
        }
    } else if (st->target_rpm == 0.0f) {
        float magnitude = (pwm_raw < 0.0f) ? -pwm_raw : pwm_raw;
        if (magnitude < cfg->pwm_dead_zone) {
            pwm_raw = 0.0f;
        }
    }

    if (pwm_raw > cfg->max_pwm) {
        pwm_raw = cfg->max_pwm;
    } else if (pwm_raw < -cfg->max_pwm) {
        pwm_raw = -cfg->max_pwm;
    }

    st->pwm_command = pwm_raw;
    hw_write_pwm(motor, pwm_raw);
}

void motor_control_step(float dt_s)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        step_one(m, dt_s);
    }
}

const MotorState *motor_control_get_state(motor_index_t motor)
{
    return &s_state[motor];
}

MotorState *motor_control_get_state_mutable(motor_index_t motor)
{
    return &s_state[motor];
}
