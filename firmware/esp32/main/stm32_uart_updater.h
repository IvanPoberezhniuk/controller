#ifndef STM32_UART_UPDATER_H
#define STM32_UART_UPDATER_H

#include "ugv_esp32_board.h"

/* Takes ownership of UART0/UART2 and does not return. UART0 first moves back
 * to GPIO43/GPIO44 so the onboard CH343 can receive a complete STM32 image. */
_Noreturn void stm32_uart_updater_run(
    const ugv_esp32_board_config_t *board);

#endif /* STM32_UART_UPDATER_H */
