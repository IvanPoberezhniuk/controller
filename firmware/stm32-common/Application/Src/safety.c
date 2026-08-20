#include "safety.h"
#include "motor_control.h"
#include "configuration.h"
#include "encoder.h"
#include "fault_manager.h"
#include "board.h"

static safety_state_t s_state;
static uint32_t s_last_command_tick_ms;
static bool s_emergency_stop_active;
static bool s_arm_requested;
static bool s_fault_clear_requested;
static bool s_estop_clear_requested;

static bool command_stale(void)
{
    return (HAL_GetTick() - s_last_command_tick_ms) > config_get()->command_timeout_ms;
}

static bool any_motor_faulted(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        const MotorState *st = motor_control_get_state(m);
        if (st->stalled || st->overcurrent || st->driver_fault || !st->encoder_valid) {
            return true;
        }
    }
    return false;
}

static bool any_target_nonzero(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        if (motor_control_get_state(m)->target_rpm != 0.0f) {
            return true;
        }
    }
    return false;
}

static bool arming_conditions_met(void)
{
    return !s_emergency_stop_active && !command_stale() && !any_motor_faulted();
}

void safety_init(void)
{
    s_state = SAFETY_STATE_BOOT;
    s_last_command_tick_ms = HAL_GetTick();
    s_emergency_stop_active = false;
    s_arm_requested = false;
    s_fault_clear_requested = false;
    s_estop_clear_requested = false;
}

void safety_notify_command_received(void)
{
    s_last_command_tick_ms = HAL_GetTick();
}

void safety_request_arm(void)
{
    if (s_state == SAFETY_STATE_DISABLED) {
        s_arm_requested = true;
    }
}

void safety_request_emergency_stop(void)
{
    s_emergency_stop_active = true;
}

void safety_clear_emergency_stop(void)
{
    if (s_state == SAFETY_STATE_EMERGENCY_STOP) {
        s_emergency_stop_active = false;
        s_estop_clear_requested = true;
    }
}

void safety_clear_fault(void)
{
    if (s_state == SAFETY_STATE_DISABLED ||
        s_state == SAFETY_STATE_DEGRADED ||
        s_state == SAFETY_STATE_FAULT) {
        s_fault_clear_requested = true;
    }
}

static void clear_latched_motor_faults(void)
{
    fault_manager_clear_latched();
    encoder_clear_latched_faults();
}

static void enable_motors_that_are_ok(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        const MotorState *st = motor_control_get_state(m);
        bool ok = !st->stalled && !st->overcurrent && !st->driver_fault && st->encoder_valid;
        motor_control_set_enabled(m, ok);
    }
}

void safety_update(void)
{
    switch (s_state) {
        case SAFETY_STATE_BOOT:
            motor_control_disable_all();
            s_arm_requested = false;
            s_fault_clear_requested = false;
            s_estop_clear_requested = false;
            s_state = SAFETY_STATE_DISABLED;
            break;

        case SAFETY_STATE_DISABLED:
            motor_control_disable_all();
            if (s_fault_clear_requested) {
                s_fault_clear_requested = false;
                s_arm_requested = false;
                clear_latched_motor_faults();
                break;
            }
            if (s_arm_requested) {
                s_arm_requested = false;
                s_state = SAFETY_STATE_ARMING;
            }
            break;

        case SAFETY_STATE_ARMING:
            motor_control_disable_all();
            if (s_emergency_stop_active) {
                s_state = SAFETY_STATE_EMERGENCY_STOP;
            } else if (command_stale()) {
                s_state = SAFETY_STATE_DISABLED;
            } else if (arming_conditions_met()) {
                s_state = SAFETY_STATE_READY;
            }
            break;

        case SAFETY_STATE_READY:
        case SAFETY_STATE_ACTIVE:
        case SAFETY_STATE_DEGRADED:
            if (s_fault_clear_requested) {
                s_fault_clear_requested = false;
                motor_control_disable_all();
                clear_latched_motor_faults();
                s_state = SAFETY_STATE_DISABLED;
                break;
            }
            if (s_emergency_stop_active) {
                motor_control_disable_all();
                s_state = SAFETY_STATE_EMERGENCY_STOP;
                break;
            }
            if (command_stale()) {
                motor_control_disable_all();
                s_state = SAFETY_STATE_FAULT;
                break;
            }
            enable_motors_that_are_ok();
            if (any_motor_faulted()) {
                s_state = SAFETY_STATE_DEGRADED;
            } else if (any_target_nonzero()) {
                s_state = SAFETY_STATE_ACTIVE;
            } else {
                s_state = SAFETY_STATE_READY;
            }
            break;

        case SAFETY_STATE_FAULT:
            motor_control_disable_all();
            if (s_fault_clear_requested) {
                s_fault_clear_requested = false;
                clear_latched_motor_faults();
                s_state = SAFETY_STATE_DISABLED;
            }
            break;

        case SAFETY_STATE_EMERGENCY_STOP:
            motor_control_disable_all();
            if (!s_emergency_stop_active && s_estop_clear_requested) {
                s_estop_clear_requested = false;
                s_state = SAFETY_STATE_DISABLED;
            }
            break;
    }
}

safety_state_t safety_get_state(void)
{
    return s_state;
}

const char *safety_state_name(safety_state_t state)
{
    switch (state) {
        case SAFETY_STATE_BOOT:            return "BOOT";
        case SAFETY_STATE_DISABLED:        return "DISABLED";
        case SAFETY_STATE_ARMING:          return "ARMING";
        case SAFETY_STATE_READY:           return "READY";
        case SAFETY_STATE_ACTIVE:          return "ACTIVE";
        case SAFETY_STATE_DEGRADED:        return "DEGRADED";
        case SAFETY_STATE_FAULT:           return "FAULT";
        case SAFETY_STATE_EMERGENCY_STOP:  return "EMERGENCY_STOP";
        default:                           return "UNKNOWN";
    }
}
