#include "timebase.h"
#include "board.h"

static const float s_dt_s = 1.0f / (float)TIMEBASE_CONTROL_LOOP_HZ;

static uint32_t tim6_input_clock_hz(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0u;
    HAL_RCC_GetClockConfig(&clocks, &flash_latency);

    uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();
    return (clocks.APB1CLKDivider == RCC_HCLK_DIV1) ? pclk1_hz : (pclk1_hz * 2u);
}

void timebase_init(void)
{
    uint32_t timer_hz = tim6_input_clock_hz();
    uint64_t max_period_hz =
        (uint64_t)TIMEBASE_CONTROL_LOOP_HZ * ((uint64_t)UINT16_MAX + 1u);
    uint32_t prescaler_div =
        (uint32_t)(((uint64_t)timer_hz + max_period_hz - 1u) / max_period_hz);
    if (prescaler_div == 0u) {
        prescaler_div = 1u;
    }

    uint32_t counter_hz = timer_hz / prescaler_div;
    uint32_t period_counts =
        (counter_hz + (TIMEBASE_CONTROL_LOOP_HZ / 2u)) / TIMEBASE_CONTROL_LOOP_HZ;

    __HAL_TIM_SET_PRESCALER(&htim6, prescaler_div - 1u);
    __HAL_TIM_SET_AUTORELOAD(&htim6, period_counts - 1u);
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
