#include "timebase.h"
#include "board.h"

/* htim6 clock = 170 MHz (HSI16 -> PLL /4 x85 /2, APB1 prescaler = 1 ->
 * timer clock = APB1 clock, not doubled). TIM6 is a 16-bit basic timer
 * (ARR max 65535), so 170,000,000 / 500 Hz = 340,000 counts no longer
 * fits with no prescaler -- PSC=169 divides down to an even 1 MHz counter
 * clock first, then ARR = clock / rate - 1 as before. */
#define TIMEBASE_TIM6_CLOCK_HZ 170000000u
#define TIMEBASE_PSC 169u
#define TIMEBASE_COUNTER_CLOCK_HZ ((TIMEBASE_TIM6_CLOCK_HZ) / (TIMEBASE_PSC + 1u))
#define TIMEBASE_ARR ((TIMEBASE_COUNTER_CLOCK_HZ / TIMEBASE_CONTROL_LOOP_HZ) - 1u)

static const float s_dt_s = 1.0f / (float)TIMEBASE_CONTROL_LOOP_HZ;

void timebase_init(void)
{
    __HAL_TIM_SET_PRESCALER(&htim6, TIMEBASE_PSC);
    __HAL_TIM_SET_AUTORELOAD(&htim6, TIMEBASE_ARR);
    __HAL_TIM_SET_COUNTER(&htim6, 0u);
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start(&htim6);
}

bool timebase_tick_ready(void)
{
    if (__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
        return true;
    }
    return false;
}

float timebase_dt_s(void)
{
    return s_dt_s;
}
