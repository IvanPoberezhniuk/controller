#ifndef APPLICATION_CURRENT_MONITOR_H
#define APPLICATION_CURRENT_MONITOR_H

void current_monitor_init(void);

/* Triggers and polls both ADC1 (MOTOR2_LIS) and ADC2 (the other five
 * current-sense channels), converts to amps, and writes MotorState.current_a
 * (max of R_IS/L_IS per motor -- whichever half-bridge is actively driving
 * is the meaningful one, the other reads ~0) via
 * motor_control_get_state_mutable(). Software-triggered/polled, not
 * DMA-driven, for this milestone; blocks for a handful of ADC conversions
 * (microseconds) so call it from the control tick, not an ISR. */
void current_monitor_sample(void);

#endif /* APPLICATION_CURRENT_MONITOR_H */
