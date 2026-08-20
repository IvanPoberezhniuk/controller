#ifndef APPLICATION_FAULT_MANAGER_H
#define APPLICATION_FAULT_MANAGER_H

void fault_manager_init(void);

/* Detects and latches overcurrent/stall faults. A confirmed fault disables
 * the affected motor and remains set until the safety state machine accepts
 * an explicit clear request. */
void fault_manager_update(void);

void fault_manager_clear_latched(void);

#endif /* APPLICATION_FAULT_MANAGER_H */
