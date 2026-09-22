#include "ugv_uart_protocol.h"

#include <string.h>

enum {
    PARSER_WAIT_SYNC_0 = 0,
    PARSER_WAIT_SYNC_1,
    PARSER_READ_BODY,
};

static void put_u16_le(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
}

static uint16_t get_u16_le(const uint8_t *input)
{
    return (uint16_t)input[0] | ((uint16_t)input[1] << 8u);
}

static void put_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
    output[2] = (uint8_t)(value >> 16u);
    output[3] = (uint8_t)(value >> 24u);
}

static uint32_t get_u32_le(const uint8_t *input)
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8u) |
           ((uint32_t)input[2] << 16u) | ((uint32_t)input[3] << 24u);
}

uint16_t ugv_uart_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFFu;
    if (data == NULL && size != 0u) {
        return 0u;
    }
    for (size_t index = 0; index < size; ++index) {
        crc ^= (uint16_t)data[index] << 8u;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u
                      ? (uint16_t)((crc << 1u) ^ 0x1021u)
                      : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}

size_t ugv_uart_encode_frame(uint8_t *output, size_t output_size,
                             uint8_t type, uint8_t sequence,
                             const uint8_t *payload, uint8_t payload_size)
{
    const size_t frame_size = 2u + 4u + payload_size + 2u;
    if (output == NULL || type == 0u ||
        payload_size > UGV_UART_MAX_PAYLOAD_SIZE ||
        (payload_size != 0u && payload == NULL) ||
        output_size < frame_size) {
        return 0u;
    }

    output[0] = UGV_UART_SYNC_0;
    output[1] = UGV_UART_SYNC_1;
    output[2] = UGV_UART_PROTOCOL_VERSION;
    output[3] = type;
    output[4] = sequence;
    output[5] = payload_size;
    if (payload_size != 0u) {
        memcpy(&output[6], payload, payload_size);
    }
    const uint16_t crc = ugv_uart_crc16(&output[2], 4u + payload_size);
    put_u16_le(&output[6u + payload_size], crc);
    return frame_size;
}

void ugv_uart_parser_init(ugv_uart_parser_t *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

static void parser_resync(ugv_uart_parser_t *parser, uint8_t byte)
{
    parser->body_size = 0u;
    parser->expected_size = 0u;
    parser->state = byte == UGV_UART_SYNC_0
                        ? PARSER_WAIT_SYNC_1
                        : PARSER_WAIT_SYNC_0;
}

bool ugv_uart_parser_push(ugv_uart_parser_t *parser, uint8_t byte,
                          ugv_uart_frame_t *frame)
{
    if (parser == NULL || frame == NULL) {
        return false;
    }

    if (parser->state == PARSER_WAIT_SYNC_0) {
        if (byte == UGV_UART_SYNC_0) {
            parser->state = PARSER_WAIT_SYNC_1;
        }
        return false;
    }
    if (parser->state == PARSER_WAIT_SYNC_1) {
        if (byte == UGV_UART_SYNC_1) {
            parser->state = PARSER_READ_BODY;
            parser->body_size = 0u;
        } else if (byte != UGV_UART_SYNC_0) {
            parser->state = PARSER_WAIT_SYNC_0;
        }
        return false;
    }

    parser->body[parser->body_size++] = byte;
    if (parser->body_size == 4u) {
        const uint8_t payload_size = parser->body[3];
        if (parser->body[0] != UGV_UART_PROTOCOL_VERSION ||
            parser->body[1] == 0u || payload_size > UGV_UART_MAX_PAYLOAD_SIZE) {
            parser->format_error_count++;
            parser_resync(parser, byte);
            return false;
        }
        parser->expected_size = (uint8_t)(4u + payload_size + 2u);
    }
    if (parser->expected_size == 0u ||
        parser->body_size < parser->expected_size) {
        return false;
    }

    const uint8_t payload_size = parser->body[3];
    const uint16_t expected_crc = get_u16_le(&parser->body[4u + payload_size]);
    const uint16_t actual_crc = ugv_uart_crc16(parser->body,
                                               4u + payload_size);
    if (actual_crc != expected_crc) {
        parser->crc_error_count++;
        parser_resync(parser, byte);
        return false;
    }

    frame->type = parser->body[1];
    frame->sequence = parser->body[2];
    frame->payload_size = payload_size;
    if (payload_size != 0u) {
        memcpy(frame->payload, &parser->body[4], payload_size);
    }
    parser_resync(parser, 0u);
    return true;
}

bool ugv_uart_encode_control(uint8_t *payload, size_t size,
                             const ugv_uart_control_t *message)
{
    if (payload == NULL || message == NULL ||
        size < UGV_UART_CONTROL_PAYLOAD_SIZE) {
        return false;
    }
    payload[0] = message->target_role;
    payload[1] = message->enabled_mask & UGV_WHEEL_ENABLE_ALL;
    payload[2] = message->flags;
    put_u16_le(&payload[3], (uint16_t)message->front_rpm);
    put_u16_le(&payload[5], (uint16_t)message->center_rpm);
    put_u16_le(&payload[7], (uint16_t)message->rear_rpm);
    return true;
}

bool ugv_uart_decode_control(ugv_uart_control_t *message,
                             const uint8_t *payload, size_t size)
{
    if (payload == NULL || message == NULL ||
        size != UGV_UART_CONTROL_PAYLOAD_SIZE) {
        return false;
    }
    message->target_role = payload[0];
    message->enabled_mask = payload[1] & UGV_WHEEL_ENABLE_ALL;
    message->flags = payload[2];
    message->front_rpm = (int16_t)get_u16_le(&payload[3]);
    message->center_rpm = (int16_t)get_u16_le(&payload[5]);
    message->rear_rpm = (int16_t)get_u16_le(&payload[7]);
    return true;
}

bool ugv_uart_encode_telemetry(uint8_t *payload, size_t size,
                               const ugv_uart_telemetry_t *message)
{
    if (payload == NULL || message == NULL ||
        size < UGV_UART_TELEMETRY_PAYLOAD_SIZE) {
        return false;
    }
    payload[0] = message->node_role;
    payload[1] = message->safety_state;
    put_u16_le(&payload[2], (uint16_t)message->front_rpm);
    put_u16_le(&payload[4], (uint16_t)message->center_rpm);
    put_u16_le(&payload[6], (uint16_t)message->rear_rpm);
    put_u16_le(&payload[8], message->front_current_ma);
    put_u16_le(&payload[10], message->center_current_ma);
    put_u16_le(&payload[12], message->rear_current_ma);
    payload[14] = message->fault_mask;
    payload[15] = message->valid_mask;
    put_u16_le(&payload[16], message->control_rx_count);
    payload[18] = message->last_control_flags;
    payload[19] = message->last_enabled_mask;
    put_u32_le(&payload[20], message->uptime_ms);
    put_u16_le(&payload[24], message->stack_free_bytes);
    return true;
}

bool ugv_uart_decode_telemetry(ugv_uart_telemetry_t *message,
                               const uint8_t *payload, size_t size)
{
    if (payload == NULL || message == NULL ||
        size != UGV_UART_TELEMETRY_PAYLOAD_SIZE) {
        return false;
    }
    message->node_role = payload[0];
    message->safety_state = payload[1];
    message->front_rpm = (int16_t)get_u16_le(&payload[2]);
    message->center_rpm = (int16_t)get_u16_le(&payload[4]);
    message->rear_rpm = (int16_t)get_u16_le(&payload[6]);
    message->front_current_ma = get_u16_le(&payload[8]);
    message->center_current_ma = get_u16_le(&payload[10]);
    message->rear_current_ma = get_u16_le(&payload[12]);
    message->fault_mask = payload[14];
    message->valid_mask = payload[15];
    message->control_rx_count = get_u16_le(&payload[16]);
    message->last_control_flags = payload[18];
    message->last_enabled_mask = payload[19];
    message->uptime_ms = get_u32_le(&payload[20]);
    message->stack_free_bytes = get_u16_le(&payload[24]);
    return true;
}
