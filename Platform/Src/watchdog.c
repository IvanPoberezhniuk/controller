#include "watchdog.h"
#include "board.h"

void watchdog_refresh(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}
