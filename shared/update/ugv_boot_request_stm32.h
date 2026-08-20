#ifndef UGV_BOOT_REQUEST_STM32_H
#define UGV_BOOT_REQUEST_STM32_H

#include <stdbool.h>

/* One-shot request stored in the eight-byte SRAM range excluded from both
 * linker scripts. It survives NVIC_SystemReset but not a power cycle. */
bool ugv_boot_request_consume(void);

/* Writes the request and performs NVIC_SystemReset(). */
_Noreturn void ugv_boot_request_set_and_reset(void);

#endif /* UGV_BOOT_REQUEST_STM32_H */
