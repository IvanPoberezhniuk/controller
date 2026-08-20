#ifndef APPLICATION_ENCODER_H
#define APPLICATION_ENCODER_H

void encoder_init(void);

/* Reads all three hardware quadrature counters, computes rollover-safe
 * deltas, and writes encoder_count/encoder_delta/measured_rpm/encoder_valid
 * into the shared MotorState owned by motor_control.c. Call once per
 * control-loop tick, same rate/phase as motor_control_step(). */
void encoder_update(float dt_s);

/* Clears a previously latched disconnected/frozen-encoder fault. Called only
 * by the safety state machine while motor outputs are disabled. */
void encoder_clear_latched_faults(void);

#endif /* APPLICATION_ENCODER_H */
