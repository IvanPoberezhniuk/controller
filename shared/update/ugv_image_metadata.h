#ifndef UGV_IMAGE_METADATA_H
#define UGV_IMAGE_METADATA_H

#include <stdbool.h>
#include <stdint.h>

#include "ugv_flash_layout.h"

#define UGV_IMAGE_METADATA_MAGIC          0x5547564du /* "UGVM" */
#define UGV_IMAGE_METADATA_FORMAT_VERSION 1u
#define UGV_IMAGE_FLAG_VALID              (1u << 0)

/* Exactly four STM32G4 flash doublewords. The first doubleword (magic through
 * node_id) is programmed last, so an interrupted metadata write cannot expose
 * the new image as valid. */
typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint8_t node_id;
    uint8_t reserved0;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t generation;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t metadata_crc32;
} ugv_image_metadata_t;

void ugv_image_metadata_create(ugv_image_metadata_t *metadata,
                               uint8_t node_id, uint32_t image_size,
                               uint32_t image_crc32, uint32_t generation);

bool ugv_image_metadata_header_valid(const ugv_image_metadata_t *metadata,
                                     uint8_t expected_node_id);

/* Checks the first two application vector words without dereferencing flash.
 * reset_handler must retain its Cortex-M Thumb bit. */
bool ugv_image_vectors_valid(uint32_t initial_stack_pointer,
                             uint32_t reset_handler);

#endif /* UGV_IMAGE_METADATA_H */
