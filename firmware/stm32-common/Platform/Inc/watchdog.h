#ifndef PLATFORM_WATCHDOG_H
#define PLATFORM_WATCHDOG_H

/* Thin wrapper around the IWDG CubeMX already configured (~512 ms timeout at
 * LSI/4, Reload=4095). Refresh after every completed control-loop iteration,
 * including intentional FAULT/EMERGENCY_STOP states. A genuinely stuck loop
 * cannot reach the refresh and will still reset. */
void watchdog_refresh(void);

#endif /* PLATFORM_WATCHDOG_H */
