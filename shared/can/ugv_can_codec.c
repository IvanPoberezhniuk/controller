#include "ugv_can_codec.h"

static void write_i16_le(uint8_t *destination, int16_t value)
{
    uint16_t raw = (uint16_t)value;
    destination[0] = (uint8_t)(raw & 0xffu);
    destination[1] = (uint8_t)((raw >> 8) & 0xffu);
}

static int16_t read_i16_le(const uint8_t *source)
{
    uint16_t raw = (uint16_t)source[0] | ((uint16_t)source[1] << 8);
    return (int16_t)raw;
}

bool ugv_can_encode_motion_cmd(uint8_t *payload, size_t payload_size,
                               const ugv_can_motion_cmd_t *message)
{
    if (payload == NULL || message == NULL || payload_size < UGV_CAN_MOTION_CMD_DLC) {
        return false;
    }

    payload[0] = message->sequence;
    payload[1] = message->mode_flags;
    write_i16_le(&payload[2], message->left_target);
    write_i16_le(&payload[4], message->right_target);
    payload[6] = message->limit;
    payload[7] = message->reserved;
    return true;
}

bool ugv_can_decode_motion_cmd(ugv_can_motion_cmd_t *message,
                               const uint8_t *payload, size_t payload_size)
{
    if (message == NULL || payload == NULL || payload_size != UGV_CAN_MOTION_CMD_DLC) {
        return false;
    }

    message->sequence = payload[0];
    message->mode_flags = payload[1];
    message->left_target = read_i16_le(&payload[2]);
    message->right_target = read_i16_le(&payload[4]);
    message->limit = payload[6];
    message->reserved = payload[7];
    return true;
}

bool ugv_can_encode_system_enable(uint8_t *payload, size_t payload_size,
                                  const ugv_can_system_enable_t *message)
{
    if (payload == NULL || message == NULL || payload_size < UGV_CAN_SYSTEM_ENABLE_DLC) {
        return false;
    }

    payload[0] = message->enabled;
    payload[1] = message->emergency_stop;
    return true;
}

bool ugv_can_decode_system_enable(ugv_can_system_enable_t *message,
                                  const uint8_t *payload, size_t payload_size)
{
    if (message == NULL || payload == NULL || payload_size != UGV_CAN_SYSTEM_ENABLE_DLC) {
        return false;
    }

    message->enabled = payload[0];
    message->emergency_stop = payload[1];
    return true;
}
