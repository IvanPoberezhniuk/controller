#ifndef PLATFORM_TIMEBASE_H
#define PLATFORM_TIMEBASE_H

#include <stdbool.h>

#define TIMEBASE_CONTROL_LOOP_HZ 500u

/* Reconfigures TIM6 (already Base_Init'd by CubeMX) to the exact tick rate
 * above and starts it. CubeMX's own MX_TIM6_Init() leaves the period at its
 * default 65535, which is not the ~500 Hz control-loop rate this firmware
 * needs -- owning the exact reload value here, rather than depending on
 * getting a CubeMX GUI field exactly right, keeps the control-loop rate a
 * single documented constant.
 *
 * TIM6's NVIC interrupt was not enabled in CubeMX, so this is polled from
 * the main loop via timebase_tick_ready() rather than interrupt-driven.
 * Still timer-paced, not an arbitrary HAL_Delay loop, per
 * ugv-stm32-firmware SKILL.md; can be upgraded to HAL_TIM_Base_Start_IT
 * later if tighter jitter is needed. */
void timebase_init(void);

/* True once per control-loop period; clears the underlying hardware flag.
 * Call once per superloop iteration. */
bool timebase_tick_ready(void);

float timebase_dt_s(void);

#endif /* PLATFORM_TIMEBASE_H */
