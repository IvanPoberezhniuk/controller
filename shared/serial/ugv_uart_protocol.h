#ifndef UGV_UART_PROTOCOL_H
#define UGV_UART_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../ugv_link_timing.h"

#define UGV_UART_BAUD_RATE              115200u
#define UGV_UART_PROTOCOL_VERSION       1u
#define UGV_UART_SYNC_0                 0xA5u
#define UGV_UART_SYNC_1                 0x5Au
#define UGV_UART_MAX_PAYLOAD_SIZE       26u
#define UGV_UART_MAX_FRAME_SIZE         (2u + 4u + UGV_UART_MAX_PAYLOAD_SIZE + 2u)
#define UGV_UART_CONTROL_PAYLOAD_SIZE   9u
#define UGV_UART_TELEMETRY_PAYLOAD_SIZE 26u
#define UGV_UART_COMMAND_PERIOD_MS      20u
#define UGV_UART_COMMAND_TIMEOUT_MS     UGV_LINK_COMMAND_TIMEOUT_MS
#define UGV_RC_LINK_TIMEOUT_MS          UGV_LINK_RC_TIMEOUT_MS

typedef enum {
    UGV_UART_MSG_CONTROL = 1,
    UGV_UART_MSG_TELEMETRY = 2,
    UGV_UART_MSG_FW_COMMAND = 0x10,
    UGV_UART_MSG_FW_DATA = 0x11,
    UGV_UART_MSG_FW_STATUS = 0x12,
} ugv_uart_message_type_t;

typedef enum {
    UGV_NODE_CODE_LEFT = 1,
    UGV_NODE_CODE_RIGHT = 2,
} ugv_node_role_t;

enum {
    UGV_WHEEL_ENABLE_FRONT = 1u << 0,
    UGV_WHEEL_ENABLE_CENTER = 1u << 1,
    UGV_WHEEL_ENABLE_REAR = 1u << 2,
    UGV_WHEEL_ENABLE_ALL = 0x07u,
};

enum {
    UGV_UART_CONTROL_FLAG_ARM = 1u << 0,
    UGV_UART_CONTROL_FLAG_ESTOP = 1u << 1,
    UGV_UART_CONTROL_FLAG_CLEAR_FAULT = 1u << 2,
};

typedef struct {
    uint8_t target_role;
    uint8_t enabled_mask;
    uint8_t flags;
    int16_t front_rpm;
    int16_t center_rpm;
    int16_t rear_rpm;
} ugv_uart_control_t;

typedef struct {
    uint8_t node_role;
    uint8_t safety_state;
    int16_t front_rpm;
    int16_t center_rpm;
    int16_t rear_rpm;
    uint16_t front_current_ma;
    uint16_t center_current_ma;
    uint16_t rear_current_ma;
    uint8_t fault_mask;
    uint8_t valid_mask;
    uint16_t control_rx_count;
    uint8_t last_control_flags;
    uint8_t last_enabled_mask;
    uint32_t uptime_ms;
    uint16_t stack_free_bytes;
} ugv_uart_telemetry_t;

typedef struct {
    uint8_t type;
    uint8_t sequence;
    uint8_t payload_size;
    uint8_t payload[UGV_UART_MAX_PAYLOAD_SIZE];
} ugv_uart_frame_t;

typedef struct {
    uint8_t state;
    uint8_t body[4u + UGV_UART_MAX_PAYLOAD_SIZE + 2u];
    uint8_t body_size;
    uint8_t expected_size;
    uint32_t crc_error_count;
    uint32_t format_error_count;
} ugv_uart_parser_t;

uint16_t ugv_uart_crc16(const uint8_t *data, size_t size);

size_t ugv_uart_encode_frame(uint8_t *output, size_t output_size,
                             uint8_t type, uint8_t sequence,
                             const uint8_t *payload, uint8_t payload_size);

void ugv_uart_parser_init(ugv_uart_parser_t *parser);
bool ugv_uart_parser_push(ugv_uart_parser_t *parser, uint8_t byte,
                          ugv_uart_frame_t *frame);

bool ugv_uart_encode_control(uint8_t *payload, size_t size,
                             const ugv_uart_control_t *message);
bool ugv_uart_decode_control(ugv_uart_control_t *message,
                             const uint8_t *payload, size_t size);
bool ugv_uart_encode_telemetry(uint8_t *payload, size_t size,
                               const ugv_uart_telemetry_t *message);
bool ugv_uart_decode_telemetry(ugv_uart_telemetry_t *message,
                               const uint8_t *payload, size_t size);

#endif /* UGV_UART_PROTOCOL_H */
