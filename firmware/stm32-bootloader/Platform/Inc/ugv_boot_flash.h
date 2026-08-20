#ifndef UGV_BOOT_FLASH_H
#define UGV_BOOT_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ugv_bootloader.h"

typedef struct {
    uint8_t node_id;
    uint8_t pending[8];
    size_t pending_size;
    uint32_t programmed_size;
    uint32_t accepted_size;
    uint32_t image_size;
    uint32_t generation;
    bool flash_unlocked;
} ugv_boot_flash_context_t;

void ugv_boot_flash_init(ugv_boot_flash_context_t *context, uint8_t node_id);
ugv_bootloader_platform_t ugv_boot_flash_platform(
    ugv_boot_flash_context_t *context);

bool ugv_boot_flash_application_valid(uint8_t node_id);
_Noreturn void ugv_boot_flash_jump_to_application(void);

#endif /* UGV_BOOT_FLASH_H */
