#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "configuration.h"
#include "encoder.h"
#include "fault_manager.h"
#include "motor_control.h"
#include "safety.h"

static uint32_t s_tick_ms;
static MotorState s_motors[UGV_MOTOR_COUNT];
static unsigned s_fault_clear_calls;
static unsigned s_encoder_clear_calls;

static const ugv_config_t s_config = {
    .command_timeout_ms = 300u,
};

uint32_t HAL_GetTick(void)
{
    return s_tick_ms;
}

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
    if (!enabled) {
        s_motors[motor].target_rpm = 0.0f;
    }
}

void motor_control_disable_all(void)
{
    for (motor_index_t motor = 0; motor < UGV_MOTOR_COUNT; motor++) {
        motor_control_set_enabled(motor, false);
    }
}

void fault_manager_clear_latched(void)
{
    s_fault_clear_calls++;
    for (motor_index_t motor = 0; motor < UGV_MOTOR_COUNT; motor++) {
        s_motors[motor].stalled = false;
        s_motors[motor].overcurrent = false;
    }
}

void encoder_clear_latched_faults(void)
{
    s_encoder_clear_calls++;
    for (motor_index_t motor = 0; motor < UGV_MOTOR_COUNT; motor++) {
        s_motors[motor].encoder_valid = true;
    }
}

static void reset_fixture(void)
{
    memset(s_motors, 0, sizeof(s_motors));
    for (motor_index_t motor = 0; motor < UGV_MOTOR_COUNT; motor++) {
        s_motors[motor].encoder_valid = true;
    }
    s_tick_ms = 0u;
    s_fault_clear_calls = 0u;
    s_encoder_clear_calls = 0u;
    safety_init();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DISABLED);
}

static void arm_to_ready(void)
{
    safety_notify_command_received();
    safety_request_arm();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_ARMING);
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_READY);
}

static void test_arm_requires_fresh_command(void)
{
    reset_fixture();
    s_tick_ms = 1000u;
    safety_request_arm();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_ARMING);
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DISABLED);
}

static void test_requests_do_not_leak_across_states(void)
{
    reset_fixture();
    arm_to_ready();

    safety_request_arm();
    safety_clear_fault();
    s_tick_ms = 301u;
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_FAULT);

    safety_update();
    assert(safety_get_state() == SAFETY_STATE_FAULT);

    safety_clear_fault();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DISABLED);
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DISABLED);
}

static void test_motor_fault_latches_until_clear(void)
{
    reset_fixture();
    arm_to_ready();

    s_motors[MOTOR_FRONT].stalled = true;
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DEGRADED);
    assert(!s_motors[MOTOR_FRONT].enabled);

    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DEGRADED);

    safety_clear_fault();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DISABLED);
    assert(!s_motors[MOTOR_FRONT].stalled);
    assert(s_fault_clear_calls == 1u);
    assert(s_encoder_clear_calls == 1u);
}

static void test_estop_requires_state_scoped_clear(void)
{
    reset_fixture();
    arm_to_ready();

    safety_clear_emergency_stop();
    safety_request_emergency_stop();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_EMERGENCY_STOP);
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_EMERGENCY_STOP);

    safety_clear_emergency_stop();
    safety_update();
    assert(safety_get_state() == SAFETY_STATE_DISABLED);
}

int main(void)
{
    test_arm_requires_fresh_command();
    test_requests_do_not_leak_across_states();
    test_motor_fault_latches_until_clear();
    test_estop_requires_state_scoped_clear();
    return 0;
}
