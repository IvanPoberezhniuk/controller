#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "configuration.h"
#include "fault_manager.h"
#include "motor_control.h"

static MotorState s_motors[UGV_MOTOR_COUNT];

static const ugv_config_t s_config = {
    .pwm_min_effective = 0.1f,
    .stall_min_duration_ms = 4u,
    .stall_current_threshold_a = 10.0f,
};

const ugv_config_t *config_get(void)
{
    return &s_config;
}

const MotorState *motor_control_get_state(motor_index_t motor)
{
    return &s_motors[motor];
}

MotorState *motor_control_get_state_mutable(motor_index_t motor)
{
    return &s_motors[motor];
}

void motor_control_set_enabled(motor_index_t motor, bool enabled)
{
    s_motors[motor].enabled = enabled;
}

static void reset_fixture(void)
{
    memset(s_motors, 0, sizeof(s_motors));
    fault_manager_init();
}

static void test_overcurrent_latches_while_disabled(void)
{
    reset_fixture();
    MotorState *motor = &s_motors[MOTOR_FRONT];
    motor->enabled = true;
    motor->current_valid = true;
    motor->current_a = 11.0f;

    fault_manager_update();
    assert(motor->overcurrent);

    motor->enabled = false;
    motor->current_a = 0.0f;
    fault_manager_update();
    assert(motor->overcurrent);

    fault_manager_clear_latched();
    assert(!motor->overcurrent);

    motor->enabled = true;
    motor->current_valid = false;
    motor->current_a = 100.0f;
    fault_manager_update();
    assert(!motor->overcurrent);
}

static void test_stall_latches_until_clear(void)
{
    reset_fixture();
    MotorState *motor = &s_motors[MOTOR_FRONT];
    motor->enabled = true;
    motor->target_rpm = 20.0f;
    motor->measured_rpm = 0.0f;
    motor->pwm_command = 0.5f;

    fault_manager_update();
    assert(!motor->stalled);
    fault_manager_update();
    assert(motor->stalled);
    assert(!motor->enabled);

    fault_manager_update();
    assert(motor->stalled);

    fault_manager_clear_latched();
    assert(!motor->stalled);
}

int main(void)
{
    test_overcurrent_latches_while_disabled();
    test_stall_latches_until_clear();
    return 0;
}
