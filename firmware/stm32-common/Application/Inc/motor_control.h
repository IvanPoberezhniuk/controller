#ifndef APPLICATION_MOTOR_CONTROL_H
#define APPLICATION_MOTOR_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_FRONT  = 0,
    MOTOR_CENTER = 1,
    MOTOR_REAR   = 2,
} motor_index_t;

/* Runtime state for one wheel and its local safety signals. */
typedef struct {
    int32_t encoder_count;
    int32_t encoder_delta;

    float measured_rpm;
    float target_rpm;

    float pwm_command;   /* signed, -1..+1 fraction of max_pwm; sign = direction */
    float current_a;
    bool current_valid;

    float pid_integral;
    float previous_error;

    bool enabled;
    bool encoder_valid;
    bool overcurrent;
    bool driver_fault;
    bool stalled;
} MotorState;

void motor_control_init(void);

/* Accepts only a finite target within config.max_target_rpm. Returns false
 * without changing the command when the motor index or target is invalid. */
bool motor_control_set_target(motor_index_t motor, float target_rpm);

/* Runs one control-loop iteration for every motor: ramp, PID, direction
 * interlock, PWM output. Must be called at a fixed rate from the TIM6-paced
 * control tick, not from an arbitrary delay loop. */
void motor_control_step(float dt_s);

/* Forces PWM output to zero and disables the driver enable pins immediately,
 * without going through the ramp. Used by safety.c on any transition out of
 * ACTIVE/READY. */
void motor_control_disable_all(void);

/* Used by fault_manager.c to disable a single motor (e.g. on stall) without
 * affecting the others. */
void motor_control_set_enabled(motor_index_t motor, bool enabled);

const MotorState *motor_control_get_state(motor_index_t motor);

/* Mutable access for encoder.c/current_monitor.c/fault_manager.c to report
 * readings and fault flags into the shared per-motor state. motor_control.c
 * owns the MotorState storage; those modules do not keep their own copies. */
MotorState *motor_control_get_state_mutable(motor_index_t motor);

#endif /* APPLICATION_MOTOR_CONTROL_H */
