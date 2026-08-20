#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ugv_crc32.h"
#include "ugv_fw_update_protocol.h"

static void test_crc32(void)
{
    static const uint8_t check[] = "123456789";
    assert(ugv_crc32(check, sizeof(check) - 1u) == 0xcbf43926u);

    uint32_t state = ugv_crc32_init();
    state = ugv_crc32_update(state, check, 4u);
    state = ugv_crc32_update(state, &check[4], sizeof(check) - 1u - 4u);
    assert(ugv_crc32_finalize(state) == 0xcbf43926u);
}

static void test_command_round_trip(void)
{
    const ugv_fw_command_t source = {
        .opcode = UGV_FW_COMMAND_BEGIN,
        .target_node = UGV_CAN_NODE_LEFT,
        .session = 0xa5u,
        .flags = 0x03u,
        .value = 0x12345678u,
    };
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    ugv_fw_command_t decoded = {0};

    assert(ugv_fw_encode_command(payload, sizeof(payload), &source));
    assert(ugv_fw_decode_command(&decoded, payload, sizeof(payload)));
    assert(memcmp(&source, &decoded, sizeof(source)) == 0);

    payload[0] = 0xffu;
    assert(!ugv_fw_decode_command(&decoded, payload, sizeof(payload)));
}

static void test_data_round_trip(void)
{
    const ugv_fw_data_t source = {
        .sequence = 0x3456u,
        .bytes = {0u, 1u, 2u, 3u, 4u, 5u},
    };
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    ugv_fw_data_t decoded = {0};

    assert(ugv_fw_encode_data(payload, sizeof(payload), &source));
    assert(ugv_fw_decode_data(&decoded, payload, sizeof(payload)));
    assert(decoded.sequence == source.sequence);
    assert(memcmp(decoded.bytes, source.bytes, sizeof(source.bytes)) == 0);
}

static void test_status_and_ids(void)
{
    const ugv_fw_status_t source = {
        .code = UGV_FW_STATUS_ERROR,
        .session = 7u,
        .detail = UGV_FW_ERROR_SEQUENCE,
        .protocol_version = UGV_FW_PROTOCOL_VERSION,
        .value = 42u,
    };
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    ugv_fw_status_t decoded = {0};
    uint16_t can_id = 0u;

    assert(ugv_fw_encode_status(payload, sizeof(payload), &source));
    assert(ugv_fw_decode_status(&decoded, payload, sizeof(payload)));
    assert(memcmp(&source, &decoded, sizeof(source)) == 0);

    assert(ugv_fw_data_id_for_node(UGV_CAN_NODE_LEFT, &can_id));
    assert(can_id == UGV_FW_CAN_ID_DATA_LEFT);
    assert(ugv_fw_status_id_for_node(UGV_CAN_NODE_RIGHT, &can_id));
    assert(can_id == UGV_FW_CAN_ID_STATUS_RIGHT);
    assert(!ugv_fw_data_id_for_node(UGV_CAN_NODE_PI_GATEWAY, &can_id));
}

int main(void)
{
    test_crc32();
    test_command_round_trip();
    test_data_round_trip();
    test_status_and_ids();
    puts("firmware update protocol tests passed");
    return 0;
}
