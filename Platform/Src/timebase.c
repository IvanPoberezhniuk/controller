#include "timebase.h"
#include "board.h"

/* htim6 clock = 16 MHz (HSI, no PLL, APB1 prescaler = 1 -> timer clock =
 * APB1 clock, not doubled). ARR = clock / rate - 1. */
#define TIMEBASE_TIM6_CLOCK_HZ 16000000u
#define TIMEBASE_ARR ((TIMEBASE_TIM6_CLOCK_HZ / TIMEBASE_CONTROL_LOOP_HZ) - 1u)

static const float s_dt_s = 1.0f / (float)TIMEBASE_CONTROL_LOOP_HZ;

void timebase_init(void)
{
    __HAL_TIM_SET_PRESCALER(&htim6, 0u);
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
