#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ugv_can_codec.h"

static void test_motion(void)
{
    const ugv_can_motion_cmd_t source = {
        .sequence = 42u,
        .mode_flags = 0x05u,
        .left_target_rpm = -320,
        .right_target_rpm = 333,
        .limit_pct = 80u,
        .reserved = 0u,
    };
    uint8_t payload[UGV_CAN_VEHICLE_MOTION_DLC] = {0};
    ugv_can_motion_cmd_t decoded = {0};

    assert(ugv_can_encode_motion_cmd(payload, sizeof(payload), &source));
    assert(payload[2] == 0xc0u && payload[3] == 0xfeu);
    assert(payload[4] == 0x4du && payload[5] == 0x01u);
    assert(ugv_can_decode_motion_cmd(&decoded, payload, sizeof(payload)));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);

    decoded.limit_pct = 101u;
    assert(!ugv_can_encode_motion_cmd(payload, sizeof(payload), &decoded));
}

static void test_system_enable(void)
{
    const ugv_can_system_enable_t source = {1u, 0u};
    uint8_t payload[UGV_CAN_SYSTEM_ENABLE_DLC] = {0};
    ugv_can_system_enable_t decoded = {0};

    assert(ugv_can_encode_system_enable(payload, sizeof(payload), &source));
    assert(ugv_can_decode_system_enable(&decoded, payload, sizeof(payload)));
    assert(decoded.enabled == 1u && decoded.emergency_stop == 0u);
    payload[1] = 2u;
    assert(!ugv_can_decode_system_enable(&decoded, payload, sizeof(payload)));
}

static void test_aux_lighting(void)
{
    const ugv_can_aux_lighting_t source = {75u, 25u, 1u, 0u};
    uint8_t payload[UGV_CAN_AUX_LIGHTING_DLC] = {0};
    ugv_can_aux_lighting_t decoded = {0};

    assert(ugv_can_encode_aux_lighting(payload, sizeof(payload), &source));
    assert(ugv_can_decode_aux_lighting(&decoded, payload, sizeof(payload)));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);
}

static void test_motor_telemetry(void)
{
    const ugv_can_motor_telemetry_t source = {7u, 4u, -10, 20, 333};
    uint8_t payload[UGV_CAN_TELEMETRY_LEFT_DLC] = {0};
    ugv_can_motor_telemetry_t decoded = {0};

    assert(ugv_can_encode_motor_telemetry(payload, sizeof(payload), &source));
    assert(ugv_can_decode_motor_telemetry(&decoded, payload, sizeof(payload)));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);
}

static void test_fault_report(void)
{
    const ugv_can_fault_report_t source = {
        9u,
        5u,
        UGV_CAN_MOTOR_FAULT_STALLED,
        UGV_CAN_MOTOR_FAULT_OVERCURRENT | UGV_CAN_MOTOR_FAULT_ENCODER,
        UGV_CAN_MOTOR_FAULT_DRIVER,
    };
    uint8_t payload[UGV_CAN_FAULT_LEFT_DLC] = {0};
    ugv_can_fault_report_t decoded = {0};

    assert(ugv_can_encode_fault_report(payload, sizeof(payload), &source));
    assert(ugv_can_decode_fault_report(&decoded, payload, sizeof(payload)));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);
}

static void test_temperatures(void)
{
    const ugv_can_temperatures_t source = {3u, 0x05u, 2345, -1000, 32767};
    uint8_t payload[UGV_CAN_TEMPS_LEFT_DLC] = {0};
    ugv_can_temperatures_t decoded = {0};

    assert(ugv_can_encode_temperatures(payload, sizeof(payload), &source));
    assert(ugv_can_decode_temperatures(&decoded, payload, sizeof(payload)));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);
    payload[1] = 0x80u;
    assert(!ugv_can_decode_temperatures(&decoded, payload, sizeof(payload)));
}

static void test_heartbeat(void)
{
    const ugv_can_heartbeat_t source = {1u, 1u, 3u, 0x55u, 0x89abcdefu};
    uint8_t payload[UGV_CAN_HEARTBEAT_LEFT_DLC] = {0};
    ugv_can_heartbeat_t decoded = {0};

    assert(ugv_can_encode_heartbeat(payload, sizeof(payload), &source));
    assert(payload[4] == 0xefu && payload[7] == 0x89u);
    assert(ugv_can_decode_heartbeat(&decoded, payload, sizeof(payload)));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);
}

static void test_invalid_arguments_and_lengths(void)
{
    uint8_t payload[UGV_CAN_VEHICLE_MOTION_DLC] = {0};
    ugv_can_motion_cmd_t message = {0};

    assert(!ugv_can_encode_motion_cmd(payload, sizeof(payload) - 1u, &message));
    assert(!ugv_can_decode_motion_cmd(&message, payload, sizeof(payload) - 1u));
    assert(!ugv_can_decode_motion_cmd(NULL, payload, sizeof(payload)));
    assert(!ugv_can_encode_motion_cmd(NULL, sizeof(payload), &message));
}

int main(void)
{
    test_motion();
    test_system_enable();
    test_aux_lighting();
    test_motor_telemetry();
    test_fault_report();
    test_temperatures();
    test_heartbeat();
    test_invalid_arguments_and_lengths();
    puts("all CAN codec tests passed");
    return 0;
}
