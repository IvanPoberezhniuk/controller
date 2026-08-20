#include "ugv_boot_request_stm32.h"

#include <stdint.h>

#include "stm32g4xx.h"
#include "ugv_flash_layout.h"

#define UGV_BOOT_REQUEST_MAGIC       0x55475642u /* "UGVB" */
#define UGV_BOOT_REQUEST_COMPLEMENT  (~UGV_BOOT_REQUEST_MAGIC)

static volatile uint32_t *request_words(void)
{
    return (volatile uint32_t *)(uintptr_t)UGV_BOOT_REQUEST_ADDRESS;
}

bool ugv_boot_request_consume(void)
{
    volatile uint32_t *words = request_words();
    const bool requested = words[0] == UGV_BOOT_REQUEST_MAGIC &&
                           words[1] == UGV_BOOT_REQUEST_COMPLEMENT;
    words[0] = 0u;
    words[1] = 0u;
    __DSB();
    return requested;
}

_Noreturn void ugv_boot_request_set_and_reset(void)
{
    volatile uint32_t *words = request_words();
    __disable_irq();
    words[0] = UGV_BOOT_REQUEST_MAGIC;
    words[1] = UGV_BOOT_REQUEST_COMPLEMENT;
    __DSB();
    __ISB();
    NVIC_SystemReset();
    for (;;) {
        /* NVIC_SystemReset does not return. */
    }
}
