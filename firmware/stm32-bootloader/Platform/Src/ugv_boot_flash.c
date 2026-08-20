#include "ugv_boot_flash.h"

#include <string.h>

#include "stm32g4xx_hal.h"
#include "ugv_crc32.h"
#include "ugv_flash_layout.h"
#include "ugv_image_metadata.h"

static uint32_t flash_page(uint32_t address)
{
    return (address - UGV_FLASH_BASE_ADDRESS) / UGV_FLASH_PAGE_SIZE;
}

static void lock_flash(ugv_boot_flash_context_t *context)
{
    if (context->flash_unlocked) {
        (void)HAL_FLASH_Lock();
        context->flash_unlocked = false;
    }
}

static bool unlock_flash(ugv_boot_flash_context_t *context)
{
    if (context->flash_unlocked) {
        return true;
    }
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    context->flash_unlocked = true;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS | FLASH_FLAG_EOP);
    return true;
}

static bool erase_pages(uint32_t first_address, uint32_t page_count)
{
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_1,
        .Page = flash_page(first_address),
        .NbPages = page_count,
    };
    uint32_t page_error = 0xffffffffu;
    return HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK;
}

static bool program_doubleword(uint32_t address, const uint8_t bytes[8])
{
    uint64_t value = 0u;
    memcpy(&value, bytes, sizeof(value));
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, value) ==
           HAL_OK;
}

static bool flush_pending(ugv_boot_flash_context_t *context, bool final)
{
    if (context->pending_size == 0u) {
        return true;
    }
    if (!final && context->pending_size < sizeof(context->pending)) {
        return true;
    }
    for (size_t index = context->pending_size;
         index < sizeof(context->pending); ++index) {
        context->pending[index] = 0xffu;
    }
    if (!program_doubleword(UGV_APP_FLASH_ADDRESS + context->programmed_size,
                            context->pending)) {
        return false;
    }
    context->programmed_size += sizeof(context->pending);
    context->pending_size = 0u;
    return true;
}

static bool flash_prepare(void *opaque, uint32_t image_size)
{
    ugv_boot_flash_context_t *context = opaque;
    if (context == NULL || image_size < 8u ||
        image_size > UGV_APP_FLASH_MAX_SIZE || !unlock_flash(context)) {
        return false;
    }

    const ugv_image_metadata_t *old_metadata =
        (const ugv_image_metadata_t *)(uintptr_t)UGV_METADATA_FLASH_ADDRESS;
    context->generation =
        ugv_image_metadata_header_valid(old_metadata, context->node_id)
            ? old_metadata->generation + 1u
            : 1u;

    /* Invalidate first. A reset after this point leaves no bootable metadata,
     * so the next boot remains in recovery instead of jumping into a partial
     * application. */
    if (!erase_pages(UGV_METADATA_FLASH_ADDRESS, 1u)) {
        lock_flash(context);
        return false;
    }

    const uint32_t app_pages =
        (image_size + UGV_FLASH_PAGE_SIZE - 1u) / UGV_FLASH_PAGE_SIZE;
    if (!erase_pages(UGV_APP_FLASH_ADDRESS, app_pages)) {
        lock_flash(context);
        return false;
    }

    memset(context->pending, 0xff, sizeof(context->pending));
    context->pending_size = 0u;
    context->programmed_size = 0u;
    context->accepted_size = 0u;
    context->image_size = image_size;
    return true;
}

static bool flash_write(void *opaque, uint32_t offset,
                        const uint8_t *data, size_t size)
{
    ugv_boot_flash_context_t *context = opaque;
    if (context == NULL || data == NULL || size == 0u ||
        offset != context->accepted_size ||
        offset + size > context->image_size || !context->flash_unlocked) {
        return false;
    }

    size_t consumed = 0u;
    while (consumed < size) {
        const size_t available = sizeof(context->pending) - context->pending_size;
        const size_t remaining = size - consumed;
        const size_t chunk = remaining < available ? remaining : available;
        memcpy(&context->pending[context->pending_size], &data[consumed], chunk);
        context->pending_size += chunk;
        consumed += chunk;
        if (!flush_pending(context, false)) {
            lock_flash(context);
            return false;
        }
    }
    context->accepted_size += (uint32_t)size;
    return true;
}

static bool write_metadata(ugv_boot_flash_context_t *context,
                           const ugv_image_metadata_t *metadata)
{
    const uint8_t *bytes = (const uint8_t *)metadata;

    /* Program doublewords 1..3 first. The first doubleword contains the magic
     * and is deliberately committed last. */
    for (size_t offset = 8u; offset < sizeof(*metadata); offset += 8u) {
        if (!program_doubleword(UGV_METADATA_FLASH_ADDRESS + (uint32_t)offset,
                                &bytes[offset])) {
            return false;
        }
    }
    return program_doubleword(UGV_METADATA_FLASH_ADDRESS, bytes);
}

static bool flash_finalize(void *opaque, uint32_t image_size,
                           uint32_t expected_crc)
{
    ugv_boot_flash_context_t *context = opaque;
    if (context == NULL || image_size != context->image_size ||
        context->accepted_size != image_size || !context->flash_unlocked ||
        !flush_pending(context, true)) {
        lock_flash(context);
        return false;
    }

    const uint8_t *application =
        (const uint8_t *)(uintptr_t)UGV_APP_FLASH_ADDRESS;
    const uint32_t *vectors =
        (const uint32_t *)(uintptr_t)UGV_APP_FLASH_ADDRESS;
    if (ugv_crc32(application, image_size) != expected_crc ||
        !ugv_image_vectors_valid(vectors[0], vectors[1])) {
        lock_flash(context);
        return false;
    }

    ugv_image_metadata_t metadata;
    ugv_image_metadata_create(&metadata, context->node_id, image_size,
                              expected_crc, context->generation);
    if (!write_metadata(context, &metadata)) {
        lock_flash(context);
        return false;
    }
    lock_flash(context);
    return ugv_image_metadata_header_valid(
        (const ugv_image_metadata_t *)(uintptr_t)UGV_METADATA_FLASH_ADDRESS,
        context->node_id);
}

static void flash_abort(void *opaque)
{
    ugv_boot_flash_context_t *context = opaque;
    if (context != NULL) {
        lock_flash(context);
        context->pending_size = 0u;
        context->accepted_size = 0u;
        context->image_size = 0u;
    }
}

static void flash_activate(void *opaque)
{
    ugv_boot_flash_context_t *context = opaque;
    if (context != NULL) {
        lock_flash(context);
    }
    NVIC_SystemReset();
}

void ugv_boot_flash_init(ugv_boot_flash_context_t *context, uint8_t node_id)
{
    if (context == NULL) {
        return;
    }
    memset(context, 0, sizeof(*context));
    context->node_id = node_id;
}

ugv_bootloader_platform_t ugv_boot_flash_platform(
    ugv_boot_flash_context_t *context)
{
    const ugv_bootloader_platform_t platform = {
        .context = context,
        .max_image_size = UGV_APP_FLASH_MAX_SIZE,
        .prepare = flash_prepare,
        .write = flash_write,
        .finalize = flash_finalize,
        .abort = flash_abort,
        .activate = flash_activate,
    };
    return platform;
}

bool ugv_boot_flash_application_valid(uint8_t node_id)
{
    const ugv_image_metadata_t *metadata =
        (const ugv_image_metadata_t *)(uintptr_t)UGV_METADATA_FLASH_ADDRESS;
    const uint32_t *vectors =
        (const uint32_t *)(uintptr_t)UGV_APP_FLASH_ADDRESS;
    const uint8_t *application =
        (const uint8_t *)(uintptr_t)UGV_APP_FLASH_ADDRESS;

    return ugv_image_metadata_header_valid(metadata, node_id) &&
           ugv_image_vectors_valid(vectors[0], vectors[1]) &&
           ugv_crc32(application, metadata->image_size) ==
               metadata->image_crc32;
}

_Noreturn void ugv_boot_flash_jump_to_application(void)
{
    const uint32_t *vectors =
        (const uint32_t *)(uintptr_t)UGV_APP_FLASH_ADDRESS;
    typedef void (*entry_point_t)(void);
    const entry_point_t entry = (entry_point_t)(uintptr_t)vectors[1];

    __disable_irq();
    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL = 0u;
    for (size_t index = 0u; index < 8u; ++index) {
        NVIC->ICER[index] = 0xffffffffu;
        NVIC->ICPR[index] = 0xffffffffu;
    }
    SCB->VTOR = UGV_APP_FLASH_ADDRESS;
    __set_MSP(vectors[0]);
    __DSB();
    __ISB();
    entry();
    for (;;) {
    }
}
