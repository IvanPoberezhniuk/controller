#ifndef APPLICATION_UART_CONTROL_SERVICE_H
#define APPLICATION_UART_CONTROL_SERVICE_H

#include <stdbool.h>

/* Owns USART2 (PA2 TX, PA3 RX) as the binary command/telemetry link to ESP32. */
void uart_control_service_init(void);
void uart_control_service_rx_isr(void);
void uart_control_service_poll(void);
void uart_control_service_publish_telemetry(void);

#endif /* APPLICATION_UART_CONTROL_SERVICE_H */
