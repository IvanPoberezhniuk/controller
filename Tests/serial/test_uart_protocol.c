#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ugv_uart_protocol.h"

static void test_control_round_trip(void)
{
    const ugv_uart_control_t source = {
        .target_role = UGV_NODE_CODE_LEFT,
        .enabled_mask = UGV_WHEEL_ENABLE_FRONT | UGV_WHEEL_ENABLE_REAR,
        .flags = UGV_UART_CONTROL_FLAG_ARM,
        .front_rpm = -333,
        .center_rpm = 0,
        .rear_rpm = 201,
    };
    uint8_t payload[UGV_UART_CONTROL_PAYLOAD_SIZE];
    uint8_t encoded[UGV_UART_MAX_FRAME_SIZE];
    assert(ugv_uart_encode_control(payload, sizeof(payload), &source));
    const size_t size = ugv_uart_encode_frame(
        encoded, sizeof(encoded), UGV_UART_MSG_CONTROL, 42u,
        payload, sizeof(payload));
    assert(size == 2u + 4u + UGV_UART_CONTROL_PAYLOAD_SIZE + 2u);

    ugv_uart_parser_t parser;
    ugv_uart_parser_init(&parser);
    ugv_uart_frame_t frame = {0};
    bool complete = false;
    for (size_t index = 0; index < size; ++index) {
        complete = ugv_uart_parser_push(&parser, encoded[index], &frame);
    }
    assert(complete);
    assert(frame.type == UGV_UART_MSG_CONTROL);
    assert(frame.sequence == 42u);

    ugv_uart_control_t decoded = {0};
    assert(ugv_uart_decode_control(&decoded, frame.payload,
                                   frame.payload_size));
    assert(decoded.target_role == source.target_role);
    assert(decoded.enabled_mask == source.enabled_mask);
    assert(decoded.flags == source.flags);
    assert(decoded.front_rpm == source.front_rpm);
    assert(decoded.center_rpm == source.center_rpm);
    assert(decoded.rear_rpm == source.rear_rpm);
}

static void test_telemetry_and_resync(void)
{
    const ugv_uart_telemetry_t source = {
        .node_role = UGV_NODE_CODE_RIGHT,
        .safety_state = 4u,
        .front_rpm = 10,
        .center_rpm = -20,
        .rear_rpm = 333,
        .front_current_ma = 120u,
        .center_current_ma = 340u,
        .rear_current_ma = 560u,
        .fault_mask = 0x02u,
        .valid_mask = 0x77u,
        .control_rx_count = 0x1234u,
        .last_control_flags = UGV_UART_CONTROL_FLAG_ARM,
        .last_enabled_mask = UGV_WHEEL_ENABLE_ALL,
    };
    uint8_t payload[UGV_UART_TELEMETRY_PAYLOAD_SIZE];
    uint8_t encoded[UGV_UART_MAX_FRAME_SIZE];
    assert(ugv_uart_encode_telemetry(payload, sizeof(payload), &source));
    const size_t size = ugv_uart_encode_frame(
        encoded, sizeof(encoded), UGV_UART_MSG_TELEMETRY, 9u,
        payload, sizeof(payload));

    ugv_uart_parser_t parser;
    ugv_uart_parser_init(&parser);
    ugv_uart_frame_t frame = {0};
    const uint8_t noise[] = {0x00u, UGV_UART_SYNC_0, 0x00u, 0xFFu};
    for (size_t index = 0; index < sizeof(noise); ++index) {
        assert(!ugv_uart_parser_push(&parser, noise[index], &frame));
    }
    for (size_t index = 0; index < size; ++index) {
        const bool complete = ugv_uart_parser_push(&parser, encoded[index], &frame);
        assert(complete == (index == size - 1u));
    }
    ugv_uart_telemetry_t decoded = {0};
    assert(ugv_uart_decode_telemetry(&decoded, frame.payload,
                                     frame.payload_size));
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);

    encoded[7] ^= 0x80u;
    for (size_t index = 0; index < size; ++index) {
        assert(!ugv_uart_parser_push(&parser, encoded[index], &frame));
    }
    assert(parser.crc_error_count == 1u);
}

int main(void)
{
    test_control_round_trip();
    test_telemetry_and_resync();
    puts("all UART protocol tests passed");
    return 0;
}
