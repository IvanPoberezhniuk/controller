#ifndef APPLICATION_FAULT_MANAGER_H
#define APPLICATION_FAULT_MANAGER_H

void fault_manager_init(void);

/* Stall detection per ugv-stm32-firmware SKILL.md: target above threshold,
 * PWM or current above threshold, measured RPM below threshold, persisting
 * for a configured duration. On confirmed stall, disables the affected
 * motor and sets MotorState.stalled -- escalation to a full-side or
 * full-vehicle stop is safety.c's decision, not this module's. */
void fault_manager_update(void);

#endif /* APPLICATION_FAULT_MANAGER_H */
