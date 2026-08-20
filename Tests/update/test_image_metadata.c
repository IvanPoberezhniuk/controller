#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ugv_can_protocol.h"
#include "ugv_flash_layout.h"
#include "ugv_image_metadata.h"

static void test_layout(void)
{
    assert(UGV_APP_FLASH_ADDRESS == 0x08006000u);
    assert(UGV_METADATA_FLASH_ADDRESS == 0x0801f800u);
    assert(UGV_APP_FLASH_MAX_SIZE == 102u * 1024u);
    assert(UGV_BOOT_REQUEST_ADDRESS == 0x20007ff8u);
}

static void test_metadata(void)
{
    ugv_image_metadata_t metadata;
    ugv_image_metadata_create(&metadata, UGV_CAN_NODE_LEFT, 43272u,
                              0x12345678u, 9u);

    assert(ugv_image_metadata_header_valid(&metadata, UGV_CAN_NODE_LEFT));
    assert(!ugv_image_metadata_header_valid(&metadata, UGV_CAN_NODE_RIGHT));

    metadata.image_crc32 ^= 1u;
    assert(!ugv_image_metadata_header_valid(&metadata, UGV_CAN_NODE_LEFT));
}

static void test_vectors(void)
{
    assert(ugv_image_vectors_valid(0x20007ff8u,
                                   UGV_APP_FLASH_ADDRESS + 0x101u));
    assert(!ugv_image_vectors_valid(0x10000000u,
                                    UGV_APP_FLASH_ADDRESS + 0x101u));
    assert(!ugv_image_vectors_valid(0x20007ff8u,
                                    UGV_APP_FLASH_ADDRESS + 0x100u));
    assert(!ugv_image_vectors_valid(0x20007ff8u,
                                    UGV_FLASH_BASE_ADDRESS + 0x101u));
}

int main(void)
{
    test_layout();
    test_metadata();
    test_vectors();
    puts("image metadata tests passed");
    return 0;
}
