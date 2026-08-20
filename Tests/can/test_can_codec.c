#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ugv_can_codec.h"

static void test_motion_round_trip(void)
{
    const ugv_can_motion_cmd_t source = {
        .sequence = 42u,
        .mode_flags = 0x05u,
        .left_target = -320,
        .right_target = 875,
        .limit = 80u,
        .reserved = 0u,
    };
    uint8_t payload[UGV_CAN_MOTION_CMD_DLC] = {0};
    ugv_can_motion_cmd_t decoded = {0};

    assert(ugv_can_encode_motion_cmd(payload, sizeof(payload), &source));
    assert(payload[2] == 0xc0u && payload[3] == 0xfeu);
    assert(payload[4] == 0x6bu && payload[5] == 0x03u);
    assert(ugv_can_decode_motion_cmd(&decoded, payload, sizeof(payload)));
    assert(decoded.sequence == source.sequence);
    assert(decoded.mode_flags == source.mode_flags);
    assert(decoded.left_target == source.left_target);
    assert(decoded.right_target == source.right_target);
    assert(decoded.limit == source.limit);
}

static void test_invalid_lengths(void)
{
    uint8_t payload[UGV_CAN_MOTION_CMD_DLC] = {0};
    ugv_can_motion_cmd_t message = {0};

    assert(!ugv_can_encode_motion_cmd(payload, sizeof(payload) - 1u, &message));
    assert(!ugv_can_decode_motion_cmd(&message, payload, sizeof(payload) - 1u));
    assert(!ugv_can_decode_motion_cmd(NULL, payload, sizeof(payload)));
}

int main(void)
{
    test_motion_round_trip();
    test_invalid_lengths();
    puts("all CAN codec tests passed");
    return 0;
}
