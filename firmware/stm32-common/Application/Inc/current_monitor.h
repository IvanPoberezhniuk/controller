#ifndef APPLICATION_CURRENT_MONITOR_H
#define APPLICATION_CURRENT_MONITOR_H

void current_monitor_init(void);

/* Reads six direct current-sense inputs (R_IS/L_IS per motor) using ADC2's
 * five-rank scan plus ADC1's PB12 channel. Writes the larger directional
 * reading to MotorState.current_a and marks all readings invalid if either
 * scan fails or UGV_CURRENT_SENSE_CALIBRATED is 0. Software-triggered/polled
 * for this milestone; call from the control tick, not an ISR. */
void current_monitor_sample(void);

#endif /* APPLICATION_CURRENT_MONITOR_H */
