#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ugv_bootloader.h"
#include "ugv_crc32.h"

#define MOCK_FLASH_SIZE 256u

typedef struct {
    uint8_t flash[MOCK_FLASH_SIZE];
    uint32_t prepared_size;
    bool metadata_valid;
    bool activated;
    bool fail_prepare;
    bool fail_write;
    bool fail_finalize;
} mock_platform_t;

static bool mock_prepare(void *context, uint32_t image_size)
{
    mock_platform_t *mock = context;
    mock->metadata_valid = false;
    mock->prepared_size = image_size;
    memset(mock->flash, 0xff, sizeof(mock->flash));
    return !mock->fail_prepare;
}

static bool mock_write(void *context, uint32_t offset,
                       const uint8_t *data, size_t size)
{
    mock_platform_t *mock = context;
    if (mock->fail_write || offset + size > sizeof(mock->flash)) {
        return false;
    }
    memcpy(&mock->flash[offset], data, size);
    return true;
}

static bool mock_finalize(void *context, uint32_t image_size,
                          uint32_t expected_crc)
{
    mock_platform_t *mock = context;
    if (mock->fail_finalize || image_size != mock->prepared_size ||
        ugv_crc32(mock->flash, image_size) != expected_crc) {
        return false;
    }
    mock->metadata_valid = true;
    return true;
}

static void mock_abort(void *context)
{
    mock_platform_t *mock = context;
    mock->metadata_valid = false;
}

static void mock_activate(void *context)
{
    mock_platform_t *mock = context;
    mock->activated = true;
}

static ugv_bootloader_platform_t make_platform(mock_platform_t *mock)
{
    const ugv_bootloader_platform_t platform = {
        .context = mock,
        .max_image_size = MOCK_FLASH_SIZE,
        .prepare = mock_prepare,
        .write = mock_write,
        .finalize = mock_finalize,
        .abort = mock_abort,
        .activate = mock_activate,
    };
    return platform;
}

static ugv_fw_command_t command(uint8_t opcode, uint8_t session,
                                uint32_t value)
{
    const ugv_fw_command_t result = {
        .opcode = opcode,
        .target_node = UGV_CAN_NODE_LEFT,
        .session = session,
        .flags = 0u,
        .value = value,
    };
    return result;
}

static void test_happy_path_and_retries(void)
{
    static const uint8_t image[] = {
        0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u,
        0x06u, 0x07u, 0x08u, 0x09u, 0x0au,
    };
    mock_platform_t mock = {0};
    ugv_bootloader_platform_t platform = make_platform(&mock);
    ugv_bootloader_t loader;
    ugv_fw_status_t status;
    ugv_bootloader_init(&loader, UGV_CAN_NODE_LEFT, &platform);

    ugv_fw_command_t begin = command(UGV_FW_COMMAND_BEGIN, 7u, sizeof(image));
    assert(ugv_bootloader_handle_command(&loader, &begin, &status));
    assert(status.code == UGV_FW_STATUS_READY);
    assert(!mock.metadata_valid);

    ugv_fw_data_t frame0 = {
        .sequence = 0u,
        .bytes = {0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u},
    };
    assert(!ugv_bootloader_handle_data(&loader, &frame0, &status));

    /* Lost status/host retry: repeated BEGIN and repeated data do not erase or
     * duplicate writes; both report the next sequence expected by the node. */
    assert(ugv_bootloader_handle_command(&loader, &begin, &status));
    assert(status.code == UGV_FW_STATUS_ACK && status.value == 1u);
    assert(ugv_bootloader_handle_data(&loader, &frame0, &status));
    assert(status.code == UGV_FW_STATUS_ACK && status.value == 1u);

    ugv_fw_data_t frame1 = {
        .sequence = 1u,
        .bytes = {0x06u, 0x07u, 0x08u, 0x09u, 0x0au, 0xffu},
    };
    assert(ugv_bootloader_handle_data(&loader, &frame1, &status));
    assert(status.code == UGV_FW_STATUS_ACK && status.value == 2u);

    const uint32_t crc = ugv_crc32(image, sizeof(image));
    ugv_fw_command_t finish = command(UGV_FW_COMMAND_FINISH, 7u, crc);
    assert(ugv_bootloader_handle_command(&loader, &finish, &status));
    assert(status.code == UGV_FW_STATUS_VERIFIED && status.value == crc);
    assert(mock.metadata_valid);

    /* FINISH is also idempotent if VERIFIED was lost. */
    assert(ugv_bootloader_handle_command(&loader, &finish, &status));
    assert(status.code == UGV_FW_STATUS_VERIFIED);

    ugv_fw_command_t activate = command(UGV_FW_COMMAND_ACTIVATE, 7u, 0u);
    assert(!ugv_bootloader_handle_command(&loader, &activate, &status));
    assert(mock.activated);
}

static void test_sequence_crc_and_target_errors(void)
{
    mock_platform_t mock = {0};
    ugv_bootloader_platform_t platform = make_platform(&mock);
    ugv_bootloader_t loader;
    ugv_fw_status_t status;
    ugv_bootloader_init(&loader, UGV_CAN_NODE_LEFT, &platform);

    ugv_fw_command_t other = command(UGV_FW_COMMAND_BEGIN, 1u, 12u);
    other.target_node = UGV_CAN_NODE_RIGHT;
    assert(!ugv_bootloader_handle_command(&loader, &other, &status));

    ugv_fw_command_t begin = command(UGV_FW_COMMAND_BEGIN, 1u, 6u);
    assert(ugv_bootloader_handle_command(&loader, &begin, &status));

    ugv_fw_data_t out_of_order = {.sequence = 2u, .bytes = {0}};
    assert(ugv_bootloader_handle_data(&loader, &out_of_order, &status));
    assert(status.code == UGV_FW_STATUS_ERROR);
    assert(status.detail == UGV_FW_ERROR_SEQUENCE);
    assert(status.value == 0u);

    ugv_fw_data_t frame = {
        .sequence = 0u,
        .bytes = {1u, 2u, 3u, 4u, 5u, 6u},
    };
    assert(ugv_bootloader_handle_data(&loader, &frame, &status));

    ugv_fw_command_t finish = command(UGV_FW_COMMAND_FINISH, 1u, 0u);
    assert(ugv_bootloader_handle_command(&loader, &finish, &status));
    assert(status.detail == UGV_FW_ERROR_CRC_MISMATCH);
    assert(!mock.metadata_valid);
}

static void test_flash_failure_is_fatal(void)
{
    mock_platform_t mock = {.fail_write = true};
    ugv_bootloader_platform_t platform = make_platform(&mock);
    ugv_bootloader_t loader;
    ugv_fw_status_t status;
    ugv_bootloader_init(&loader, UGV_CAN_NODE_LEFT, &platform);

    ugv_fw_command_t begin = command(UGV_FW_COMMAND_BEGIN, 3u, 6u);
    assert(ugv_bootloader_handle_command(&loader, &begin, &status));
    ugv_fw_data_t frame = {.sequence = 0u, .bytes = {0}};
    assert(ugv_bootloader_handle_data(&loader, &frame, &status));
    assert(status.detail == UGV_FW_ERROR_FLASH_WRITE);
    assert(loader.state == UGV_BOOTLOADER_FAILED);
}

int main(void)
{
    test_happy_path_and_retries();
    test_sequence_crc_and_target_errors();
    test_flash_failure_is_fatal();
    puts("bootloader state-machine tests passed");
    return 0;
}
