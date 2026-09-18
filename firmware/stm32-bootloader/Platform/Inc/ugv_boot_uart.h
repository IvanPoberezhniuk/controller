#ifndef UGV_BOOT_UART_H
#define UGV_BOOT_UART_H

#include <stdbool.h>

#include "ugv_fw_update_protocol.h"
#include "ugv_uart_protocol.h"

/* Bootloader transport on the same USART2 link used by the application:
 * PA2 TX -> ESP RX, PA3 RX <- ESP TX, 115200 8N1. */
bool ugv_boot_uart_init(void);
bool ugv_boot_uart_receive(ugv_uart_frame_t *frame);
bool ugv_boot_uart_send_status(const ugv_fw_status_t *status);

#endif /* UGV_BOOT_UART_H */
