#ifndef APPLICATION_SAFETY_H
#define APPLICATION_SAFETY_H

#include <stdbool.h>

/* Motor outputs are forced disabled in BOOT/DISABLED/ARMING/FAULT/
 * EMERGENCY_STOP; only READY/ACTIVE/DEGRADED allow motor_control to drive
 * outputs. */
typedef enum {
    SAFETY_STATE_BOOT = 0,
    SAFETY_STATE_DISABLED,
    SAFETY_STATE_ARMING,
    SAFETY_STATE_READY,
    SAFETY_STATE_ACTIVE,
    SAFETY_STATE_DEGRADED,
    SAFETY_STATE_FAULT,
    SAFETY_STATE_EMERGENCY_STOP,
} safety_state_t;

void safety_init(void);

/* Runs one state-machine tick. Call once per control-loop tick, before
 * motor_control_step() so a state change takes effect the same tick. */
void safety_update(void);

safety_state_t safety_get_state(void);
const char *safety_state_name(safety_state_t state);

/* Stand-in for "valid CAN communication" / "fresh command source" until the
 * CAN milestone: call this whenever a valid command is received. */
void safety_notify_command_received(void);

/* Explicit re-arm request. After any reset or fault, the system stays
 * disabled and never auto-arms or restores a previous command -- this must
 * be called deliberately (e.g. from a UART command) to begin arming. */
void safety_request_arm(void);

void safety_request_emergency_stop(void);

/* Clears the emergency-stop latch. Does not by itself re-arm -- the system
 * still returns to DISABLED and requires safety_request_arm() again. */
void safety_clear_emergency_stop(void);

/* Clears latched motor/encoder faults from DEGRADED/FAULT/DISABLED after the
 * cause has been addressed. Returns to or remains in DISABLED and never
 * re-arms by itself. */
void safety_clear_fault(void);

#endif /* APPLICATION_SAFETY_H */
