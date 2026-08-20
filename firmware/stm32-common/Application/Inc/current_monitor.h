#ifndef APPLICATION_CURRENT_MONITOR_H
#define APPLICATION_CURRENT_MONITOR_H

void current_monitor_init(void);

/* Steps a CD74HC4067 16-channel analog mux through the 6 populated
 * current-sense channels (2 per motor, R_IS/L_IS) via ADC2's single MUX_SIG
 * input, converts to amps, and writes MotorState.current_a (max of
 * R_IS/L_IS per motor -- whichever half-bridge is actively driving is the
 * meaningful one, the other reads ~0) via motor_control_get_state_mutable().
 * Software-triggered/polled, not DMA-driven, for this milestone; blocks for
 * a handful of ADC conversions plus mux settle time (microseconds) so call
 * it from the control tick, not an ISR. Sampling is a safe no-op while the
 * selected target has UGV_MUX_GPIO_CONFIGURED=0. */
void current_monitor_sample(void);

#endif /* APPLICATION_CURRENT_MONITOR_H */
