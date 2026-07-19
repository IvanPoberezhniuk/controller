#ifndef APPLICATION_APP_MAIN_H
#define APPLICATION_APP_MAIN_H

/* Called once from Core/Src/main.c's USER CODE BEGIN 2, after HAL_Init/
 * SystemClock_Config/all MX_*_Init() have run. Owns every Application/
 * Platform module's init so main.c stays a thin generated shell. */
void app_main_init(void);

/* Called every iteration of Core/Src/main.c's while(1). Internally paced by
 * timebase_tick_ready() -- returns immediately between ticks. */
void app_main_run(void);

/* Called from USART2_IRQHandler (Core/Src/stm32g4xx_it.c) on RXNE. Captures
 * RDR into a ring buffer so console bytes survive control-loop ticks that
 * run longer than a byte interval; the main loop drains it in poll_uart_rx. */
void app_main_uart2_rx_isr(void);

#endif /* APPLICATION_APP_MAIN_H */
