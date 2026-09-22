#include "stack_watermark.h"

#include <string.h>

/* Linker symbols from STM32G431xx_FLASH.ld / STM32G431XX_OTA_APP.ld:
 * `_estack` is the top of RAM (initial MSP value); `_sstack` is
 * `_estack - _Min_Stack_Size`, i.e. the lowest address the linker's own RAM
 * budget assumes the stack may reach. These are link-time addresses, not
 * variables -- take their address, never their "value". */
extern uint32_t _estack;
extern uint32_t _sstack;

#define STACK_CANARY_PATTERN 0xAAu

/* Bytes below the stack pointer, at fill time, that are left untouched.
 * app_main_init() calls stack_watermark_init() as its first statement, so
 * the live call chain at that point is just main() -> app_main_init(), a
 * few words deep; 64 bytes is generous headroom for that plus whatever the
 * compiler reserves in the current frame for locals/spills. */
#define STACK_CANARY_MARGIN_BYTES 64u

void stack_watermark_init(void)
{
    uint8_t *const stack_bottom = (uint8_t *)&_sstack;
    uint8_t *current_sp;

    __asm volatile("mov %0, sp" : "=r"(current_sp));

    uint8_t *const fill_end = current_sp - STACK_CANARY_MARGIN_BYTES;
    if (fill_end > stack_bottom) {
        (void)memset(stack_bottom, (int)STACK_CANARY_PATTERN,
                     (size_t)(fill_end - stack_bottom));
    }
}

uint16_t stack_watermark_free_bytes(void)
{
    const uint8_t *const stack_bottom = (const uint8_t *)&_sstack;
    const uint8_t *const stack_top = (const uint8_t *)&_estack;
    const uint8_t *p = stack_bottom;
    uint32_t count = 0u;

    while (p < stack_top && *p == STACK_CANARY_PATTERN) {
        ++p;
        ++count;
    }
    if (count > 0xFFFFu) {
        count = 0xFFFFu;
    }
    return (uint16_t)count;
}
