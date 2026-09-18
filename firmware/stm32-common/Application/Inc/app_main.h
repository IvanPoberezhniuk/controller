#ifndef APPLICATION_APP_MAIN_H
#define APPLICATION_APP_MAIN_H

#include <stdbool.h>

/* Called once from Core/Src/main.c's USER CODE BEGIN 2, after HAL_Init/
 * SystemClock_Config/all MX_*_Init() have run. Owns every Application/
 * Platform module's init so main.c stays a thin generated shell. */
void app_main_init(void);

/* Called every iteration of Core/Src/main.c's while(1). Internally paced by
 * timebase_tick_ready() -- returns immediately between ticks. */
void app_main_run(void);

/* Called from USART2_IRQHandler (Core/Src/stm32g4xx_it.c) on RXNE. Captures
 * binary ESP32 link bytes into the service ring buffer. */
void app_main_uart2_rx_isr(void);

#endif /* APPLICATION_APP_MAIN_H */
