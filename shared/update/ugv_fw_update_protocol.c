#include "ugv_fw_update_protocol.h"

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)(value >> 8u);
}

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8u) & 0xffu);
    destination[2] = (uint8_t)((value >> 16u) & 0xffu);
    destination[3] = (uint8_t)(value >> 24u);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8u) |
           ((uint32_t)source[2] << 16u) |
           ((uint32_t)source[3] << 24u);
}

static bool opcode_valid(uint8_t opcode)
{
    switch ((ugv_fw_command_opcode_t)opcode) {
        case UGV_FW_COMMAND_ENTER:
        case UGV_FW_COMMAND_QUERY:
        case UGV_FW_COMMAND_BEGIN:
        case UGV_FW_COMMAND_FINISH:
        case UGV_FW_COMMAND_ACTIVATE:
        case UGV_FW_COMMAND_ABORT:
            return true;
        default:
            return false;
    }
}

static bool target_valid(uint8_t node_id)
{
    return node_id == UGV_CAN_NODE_LEFT ||
           node_id == UGV_CAN_NODE_RIGHT ||
           node_id == UGV_FW_BROADCAST_NODE;
}

bool ugv_fw_encode_command(uint8_t *payload, size_t size,
                           const ugv_fw_command_t *message)
{
    if (payload == NULL || message == NULL || size < UGV_FW_FRAME_DLC ||
        !opcode_valid(message->opcode) || !target_valid(message->target_node)) {
        return false;
    }
    payload[0] = message->opcode;
    payload[1] = message->target_node;
    payload[2] = message->session;
    payload[3] = message->flags;
    write_u32_le(&payload[4], message->value);
    return true;
}

bool ugv_fw_decode_command(ugv_fw_command_t *message,
                           const uint8_t *payload, size_t size)
{
    if (message == NULL || payload == NULL || size != UGV_FW_FRAME_DLC ||
        !opcode_valid(payload[0]) || !target_valid(payload[1])) {
        return false;
    }
    message->opcode = payload[0];
    message->target_node = payload[1];
    message->session = payload[2];
    message->flags = payload[3];
    message->value = read_u32_le(&payload[4]);
    return true;
}

bool ugv_fw_encode_data(uint8_t *payload, size_t size,
                        const ugv_fw_data_t *message)
{
    if (payload == NULL || message == NULL || size < UGV_FW_FRAME_DLC) {
        return false;
    }
    write_u16_le(payload, message->sequence);
    for (size_t index = 0; index < UGV_FW_DATA_BYTES_PER_FRAME; ++index) {
        payload[index + 2u] = message->bytes[index];
    }
    return true;
}

bool ugv_fw_decode_data(ugv_fw_data_t *message,
                        const uint8_t *payload, size_t size)
{
    if (message == NULL || payload == NULL || size != UGV_FW_FRAME_DLC) {
        return false;
    }
    message->sequence = read_u16_le(payload);
    for (size_t index = 0; index < UGV_FW_DATA_BYTES_PER_FRAME; ++index) {
        message->bytes[index] = payload[index + 2u];
    }
    return true;
}

bool ugv_fw_encode_status(uint8_t *payload, size_t size,
                          const ugv_fw_status_t *message)
{
    if (payload == NULL || message == NULL || size < UGV_FW_FRAME_DLC) {
        return false;
    }
    payload[0] = message->code;
    payload[1] = message->session;
    payload[2] = message->detail;
    payload[3] = message->protocol_version;
    write_u32_le(&payload[4], message->value);
    return true;
}

bool ugv_fw_decode_status(ugv_fw_status_t *message,
                          const uint8_t *payload, size_t size)
{
    if (message == NULL || payload == NULL || size != UGV_FW_FRAME_DLC) {
        return false;
    }
    message->code = payload[0];
    message->session = payload[1];
    message->detail = payload[2];
    message->protocol_version = payload[3];
    message->value = read_u32_le(&payload[4]);
    return true;
}

bool ugv_fw_data_id_for_node(uint8_t node_id, uint16_t *can_id)
{
    if (can_id == NULL) {
        return false;
    }
    if (node_id == UGV_CAN_NODE_LEFT) {
        *can_id = UGV_FW_CAN_ID_DATA_LEFT;
        return true;
    }
    if (node_id == UGV_CAN_NODE_RIGHT) {
        *can_id = UGV_FW_CAN_ID_DATA_RIGHT;
        return true;
    }
    return false;
}

bool ugv_fw_status_id_for_node(uint8_t node_id, uint16_t *can_id)
{
    if (can_id == NULL) {
        return false;
    }
    if (node_id == UGV_CAN_NODE_LEFT) {
        *can_id = UGV_FW_CAN_ID_STATUS_LEFT;
        return true;
    }
    if (node_id == UGV_CAN_NODE_RIGHT) {
        *can_id = UGV_FW_CAN_ID_STATUS_RIGHT;
        return true;
    }
    return false;
}
