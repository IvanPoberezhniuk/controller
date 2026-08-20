#include "ugv_image_metadata.h"

#include <stddef.h>
#include <string.h>

#include "ugv_crc32.h"

_Static_assert(sizeof(ugv_image_metadata_t) == 32u,
               "metadata must occupy four flash doublewords");

static uint32_t metadata_crc(const ugv_image_metadata_t *metadata)
{
    return ugv_crc32((const uint8_t *)metadata,
                     offsetof(ugv_image_metadata_t, metadata_crc32));
}

void ugv_image_metadata_create(ugv_image_metadata_t *metadata,
                               uint8_t node_id, uint32_t image_size,
                               uint32_t image_crc32, uint32_t generation)
{
    if (metadata == NULL) {
        return;
    }
    memset(metadata, 0, sizeof(*metadata));
    metadata->magic = UGV_IMAGE_METADATA_MAGIC;
    metadata->format_version = UGV_IMAGE_METADATA_FORMAT_VERSION;
    metadata->node_id = node_id;
    metadata->image_size = image_size;
    metadata->image_crc32 = image_crc32;
    metadata->generation = generation;
    metadata->flags = UGV_IMAGE_FLAG_VALID;
    metadata->metadata_crc32 = metadata_crc(metadata);
}

bool ugv_image_metadata_header_valid(const ugv_image_metadata_t *metadata,
                                     uint8_t expected_node_id)
{
    if (metadata == NULL || metadata->magic != UGV_IMAGE_METADATA_MAGIC ||
        metadata->format_version != UGV_IMAGE_METADATA_FORMAT_VERSION ||
        metadata->node_id != expected_node_id ||
        metadata->reserved0 != 0u || metadata->reserved1 != 0u ||
        metadata->image_size < 8u ||
        metadata->image_size > UGV_APP_FLASH_MAX_SIZE ||
        metadata->flags != UGV_IMAGE_FLAG_VALID) {
        return false;
    }
    return metadata->metadata_crc32 == metadata_crc(metadata);
}

bool ugv_image_vectors_valid(uint32_t initial_stack_pointer,
                             uint32_t reset_handler)
{
    const uint32_t sram_end = UGV_SRAM_BASE_ADDRESS + UGV_USABLE_SRAM_SIZE;
    const uint32_t reset_address = reset_handler & ~1u;

    return initial_stack_pointer >= UGV_SRAM_BASE_ADDRESS &&
           initial_stack_pointer <= sram_end &&
           (initial_stack_pointer & 0x7u) == 0u &&
           (reset_handler & 1u) != 0u &&
           reset_address >= UGV_APP_FLASH_ADDRESS &&
           reset_address < UGV_METADATA_FLASH_ADDRESS;
}
