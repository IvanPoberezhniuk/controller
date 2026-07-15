#ifndef PLATFORM_WATCHDOG_H
#define PLATFORM_WATCHDOG_H

/* Thin wrapper around the IWDG CubeMX already configured (~512 ms timeout at
 * LSI/4, Reload=4095 -- see UGV_Controllers.ioc). Refresh must only be
 * called after a control-loop iteration completes successfully; a stuck
 * loop must stop refreshing and let the MCU reset, per
 * ugv-stm32-firmware SKILL.md. */
void watchdog_refresh(void);

#endif /* PLATFORM_WATCHDOG_H */
