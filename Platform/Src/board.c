#include "board.h"

static board_reset_reason_t s_reset_reason = BOARD_RESET_UNKNOWN;

void board_init(void)
{
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        s_reset_reason = BOARD_RESET_IWDG;
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
        s_reset_reason = BOARD_RESET_WWDG;
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) {
        s_reset_reason = BOARD_RESET_LOW_POWER;
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
        s_reset_reason = BOARD_RESET_SOFTWARE;
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
        s_reset_reason = BOARD_RESET_PIN;
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) {
        s_reset_reason = BOARD_RESET_POWER_ON;
    } else {
        s_reset_reason = BOARD_RESET_UNKNOWN;
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

board_reset_reason_t board_get_reset_reason(void)
{
    return s_reset_reason;
}

const char *board_reset_reason_name(board_reset_reason_t reason)
{
    switch (reason) {
        case BOARD_RESET_POWER_ON:  return "power-on";
        case BOARD_RESET_PIN:       return "pin/NRST";
        case BOARD_RESET_SOFTWARE:  return "software";
        case BOARD_RESET_IWDG:      return "iwdg-timeout";
        case BOARD_RESET_WWDG:      return "wwdg-timeout";
        case BOARD_RESET_LOW_POWER: return "low-power";
        default:                    return "unknown";
    }
}
