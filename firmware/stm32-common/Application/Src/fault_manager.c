#include "fault_manager.h"
#include "motor_control.h"
#include "configuration.h"
#include "timebase.h"

/* Algorithm-internal epsilons, not deployment tunables. */
#define STALL_TARGET_RPM_MIN    5.0f
#define STALL_MEASURED_RPM_MAX  2.0f

static uint32_t s_stall_ticks[UGV_MOTOR_COUNT];

void fault_manager_init(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        s_stall_ticks[m] = 0u;
    }
}

void fault_manager_update(void)
{
    const ugv_config_t *cfg = config_get();
    uint32_t stall_ticks_threshold =
        (cfg->stall_min_duration_ms * TIMEBASE_CONTROL_LOOP_HZ) / 1000u;

    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        MotorState *st = motor_control_get_state_mutable(m);

        if (!st->enabled) {
            s_stall_ticks[m] = 0u;
            continue;
        }

        float target_mag   = (st->target_rpm < 0.0f) ? -st->target_rpm : st->target_rpm;
        float measured_mag = (st->measured_rpm < 0.0f) ? -st->measured_rpm : st->measured_rpm;
        float pwm_mag       = (st->pwm_command < 0.0f) ? -st->pwm_command : st->pwm_command;

        if (st->current_valid && st->current_a > cfg->stall_current_threshold_a) {
            st->overcurrent = true;
        }

        bool driving_but_not_turning =
            (target_mag > STALL_TARGET_RPM_MIN) &&
            (pwm_mag > cfg->pwm_min_effective || st->overcurrent) &&
            (measured_mag < STALL_MEASURED_RPM_MAX);

        if (driving_but_not_turning) {
            if (s_stall_ticks[m] < stall_ticks_threshold) {
                s_stall_ticks[m]++;
            }
        } else {
            s_stall_ticks[m] = 0u;
        }

        if (s_stall_ticks[m] >= stall_ticks_threshold) {
            st->stalled = true;
            motor_control_set_enabled(m, false);
        }
    }
}

void fault_manager_clear_latched(void)
{
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        MotorState *st = motor_control_get_state_mutable(m);
        s_stall_ticks[m] = 0u;
        st->stalled = false;
        st->overcurrent = false;
    }
}
