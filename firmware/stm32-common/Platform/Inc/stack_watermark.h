#ifndef PLATFORM_STACK_WATERMARK_H
#define PLATFORM_STACK_WATERMARK_H

#include <stdint.h>

/* Canary-fill stack high-water-mark measurement.
 *
 * There is no FreeRTOS/heap on this target (bare-metal, no dynamic
 * allocation), so stack usage cannot be read from an RTOS task-control
 * block. Instead this fills the currently-unused portion of the main stack
 * (the region below the live call frame at init time, down to the linker's
 * `_sstack` symbol) with a known pattern, leaving a safety margin below the
 * stack pointer at fill time so nothing near the live frame is touched.
 * Later, `stack_watermark_free_bytes()` scans from the bottom of the stack
 * region upward and counts untouched pattern bytes, giving a lower bound on
 * how much stack has never been used since boot (i.e. free headroom under
 * worst observed usage). This never reads above `_estack`/below `_sstack`,
 * both defined by STM32G431xx_FLASH.ld / STM32G431XX_OTA_APP.ld.
 */

/* Must be called once, early in app_main_init(), before deeper call chains
 * or interrupts (IRQs still globally disabled) consume more stack than the
 * margin below the current stack pointer. Calling it later only makes the
 * measurement more conservative (less of the stack gets canary-filled), it
 * cannot corrupt anything since fills always stay strictly below the
 * current SP minus the margin. */
void stack_watermark_init(void);

/* Returns a lower-bound estimate, in bytes, of stack space never touched
 * since `stack_watermark_init()` ran. Saturates at UINT16_MAX. Safe to call
 * repeatedly (e.g. from telemetry publication); it only reads memory. */
uint16_t stack_watermark_free_bytes(void);

#endif /* PLATFORM_STACK_WATERMARK_H */
