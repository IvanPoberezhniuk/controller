#ifndef PLATFORM_TIMEBASE_H
#define PLATFORM_TIMEBASE_H

#include <stdbool.h>

#define TIMEBASE_CONTROL_LOOP_HZ 500u

/* Reconfigures TIM6 (already Base_Init'd by CubeMX) to the closest integer
 * tick rate above and starts it. Prescaler/ARR are derived from the live APB1
 * clock, so a SYSCLK change cannot leave a duplicated frequency literal stale.
 *
 * TIM6's NVIC interrupt was not enabled in CubeMX, so this is polled from
 * the main loop via timebase_tick_ready() rather than interrupt-driven.
 * Still timer-paced rather than delayed from the superloop; it can move to a
 * timer interrupt later if tighter jitter is needed. */
void timebase_init(void);

/* True once per control-loop period; clears the underlying hardware flag.
 * Call once per superloop iteration. */
bool timebase_tick_ready(void);

float timebase_dt_s(void);

#endif /* PLATFORM_TIMEBASE_H */
