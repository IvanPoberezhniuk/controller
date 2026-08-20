#ifndef UGV_FLASH_LAYOUT_H
#define UGV_FLASH_LAYOUT_H

#include <stdint.h>

/* STM32G431CBT6: 128 KiB single-bank flash, 2 KiB erase pages. */
#define UGV_FLASH_BASE_ADDRESS       0x08000000u
#define UGV_FLASH_TOTAL_SIZE         (128u * 1024u)
#define UGV_FLASH_PAGE_SIZE          2048u

/* Twelve pages are reserved for the immutable bootloader. The final page is
 * metadata, leaving 102 KiB for the role-specific motor application. */
#define UGV_BOOTLOADER_FLASH_SIZE    (24u * 1024u)
#define UGV_APP_FLASH_ADDRESS        (UGV_FLASH_BASE_ADDRESS + UGV_BOOTLOADER_FLASH_SIZE)
#define UGV_METADATA_FLASH_ADDRESS   (UGV_FLASH_BASE_ADDRESS + UGV_FLASH_TOTAL_SIZE - UGV_FLASH_PAGE_SIZE)
#define UGV_APP_FLASH_MAX_SIZE       (UGV_METADATA_FLASH_ADDRESS - UGV_APP_FLASH_ADDRESS)

#define UGV_SRAM_BASE_ADDRESS        0x20000000u
#define UGV_SRAM_TOTAL_SIZE          (32u * 1024u)

/* Two words at the top of SRAM survive NVIC_SystemReset because startup code
 * does not clear this reserved range. The application uses them to request
 * entry into the bootloader without touching flash. */
#define UGV_BOOT_REQUEST_ADDRESS     (UGV_SRAM_BASE_ADDRESS + UGV_SRAM_TOTAL_SIZE - 8u)
#define UGV_USABLE_SRAM_SIZE         (UGV_SRAM_TOTAL_SIZE - 8u)

#if ((UGV_BOOTLOADER_FLASH_SIZE % UGV_FLASH_PAGE_SIZE) != 0)
#error "Bootloader region must end on a flash page boundary"
#endif

#if ((UGV_APP_FLASH_ADDRESS % 0x200u) != 0)
#error "Application vector table must be aligned for Cortex-M4 VTOR"
#endif

#endif /* UGV_FLASH_LAYOUT_H */
