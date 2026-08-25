#include "ugv_can_codec.h"

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
}

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static void write_i16_le(uint8_t *destination, int16_t value)
{
    write_u16_le(destination, (uint16_t)value);
}

static int16_t read_i16_le(const uint8_t *source)
{
    return (int16_t)read_u16_le(source);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
    destination[2] = (uint8_t)((value >> 16) & 0xffu);
    destination[3] = (uint8_t)((value >> 24) & 0xffu);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

static bool can_encode(const void *payload, size_t size, const void *message,
                       size_t required_size)
{
    return payload != NULL && message != NULL && size >= required_size;
}

static bool can_decode(const void *message, const void *payload, size_t size,
                       size_t required_size)
{
    return message != NULL && payload != NULL && size == required_size;
}

bool ugv_can_encode_motion_cmd(uint8_t *payload, size_t size,
                               const ugv_can_motion_cmd_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_VEHICLE_MOTION_DLC) ||
        message->limit_pct > 100u) {
        return false;
    }
    payload[0] = message->sequence;
    payload[1] = message->mode_flags;
    write_i16_le(&payload[2], message->left_target_rpm);
    write_i16_le(&payload[4], message->right_target_rpm);
    payload[6] = message->limit_pct;
    payload[7] = message->reserved;
    return true;
}

bool ugv_can_decode_motion_cmd(ugv_can_motion_cmd_t *message,
                               const uint8_t *payload, size_t size)
{
    if (!can_decode(message, payload, size, UGV_CAN_VEHICLE_MOTION_DLC) ||
        payload[6] > 100u) {
        return false;
    }
    message->sequence = payload[0];
    message->mode_flags = payload[1];
    message->left_target_rpm = read_i16_le(&payload[2]);
    message->right_target_rpm = read_i16_le(&payload[4]);
    message->limit_pct = payload[6];
    message->reserved = payload[7];
    return true;
}

bool ugv_can_encode_system_enable(uint8_t *payload, size_t size,
                                  const ugv_can_system_enable_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_SYSTEM_ENABLE_DLC) ||
        message->enabled > 1u || message->emergency_stop > 1u) {
        return false;
    }
    payload[0] = message->enabled;
    payload[1] = message->emergency_stop;
    return true;
}

bool ugv_can_decode_system_enable(ugv_can_system_enable_t *message,
                                  const uint8_t *payload, size_t size)
{
    if (!can_decode(message, payload, size, UGV_CAN_SYSTEM_ENABLE_DLC) ||
        payload[0] > 1u || payload[1] > 1u) {
        return false;
    }
    message->enabled = payload[0];
    message->emergency_stop = payload[1];
    return true;
}

bool ugv_can_encode_aux_lighting(uint8_t *payload, size_t size,
                                 const ugv_can_aux_lighting_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_AUX_LIGHTING_DLC) ||
        message->front_pct > 100u || message->rear_pct > 100u ||
        message->left_indicator > 1u || message->right_indicator > 1u) {
        return false;
    }
    payload[0] = message->front_pct;
    payload[1] = message->rear_pct;
    payload[2] = message->left_indicator;
    payload[3] = message->right_indicator;
    return true;
}

bool ugv_can_decode_aux_lighting(ugv_can_aux_lighting_t *message,
                                 const uint8_t *payload, size_t size)
{
    if (!can_decode(message, payload, size, UGV_CAN_AUX_LIGHTING_DLC) ||
        payload[0] > 100u || payload[1] > 100u ||
        payload[2] > 1u || payload[3] > 1u) {
        return false;
    }
    message->front_pct = payload[0];
    message->rear_pct = payload[1];
    message->left_indicator = payload[2];
    message->right_indicator = payload[3];
    return true;
}

static bool control_status_valid(const ugv_can_control_status_t *message)
{
    return message->selected_mode <= UGV_CAN_CONTROL_MODE_AUTO &&
           message->active_source <= UGV_CAN_CONTROL_SOURCE_PI &&
           message->enabled <= 1u && message->rc_link_up <= 1u &&
           message->rc_failsafe <= 1u && message->link_quality_pct <= 100u;
}

bool ugv_can_encode_control_status(uint8_t *payload, size_t size,
                                   const ugv_can_control_status_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_CONTROL_STATUS_DLC) ||
        !control_status_valid(message)) {
        return false;
    }
    payload[0] = message->sequence;
    payload[1] = message->selected_mode;
    payload[2] = message->active_source;
    payload[3] = message->enabled;
    payload[4] = message->rc_link_up;
    payload[5] = message->rc_failsafe;
    payload[6] = message->link_quality_pct;
    payload[7] = (uint8_t)message->rssi_dbm;
    return true;
}

bool ugv_can_decode_control_status(ugv_can_control_status_t *message,
                                   const uint8_t *payload, size_t size)
{
    ugv_can_control_status_t decoded;

    if (!can_decode(message, payload, size, UGV_CAN_CONTROL_STATUS_DLC)) {
        return false;
    }
    decoded.sequence = payload[0];
    decoded.selected_mode = payload[1];
    decoded.active_source = payload[2];
    decoded.enabled = payload[3];
    decoded.rc_link_up = payload[4];
    decoded.rc_failsafe = payload[5];
    decoded.link_quality_pct = payload[6];
    decoded.rssi_dbm = (int8_t)payload[7];
    if (!control_status_valid(&decoded)) {
        return false;
    }
    *message = decoded;
    return true;
}

bool ugv_can_encode_motor_telemetry(uint8_t *payload, size_t size,
                                    const ugv_can_motor_telemetry_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_TELEMETRY_LEFT_DLC)) {
        return false;
    }
    payload[0] = message->sequence;
    payload[1] = message->safety_state;
    write_i16_le(&payload[2], message->front_rpm);
    write_i16_le(&payload[4], message->center_rpm);
    write_i16_le(&payload[6], message->rear_rpm);
    return true;
}

bool ugv_can_decode_motor_telemetry(ugv_can_motor_telemetry_t *message,
                                    const uint8_t *payload, size_t size)
{
    if (!can_decode(message, payload, size, UGV_CAN_TELEMETRY_LEFT_DLC)) {
        return false;
    }
    message->sequence = payload[0];
    message->safety_state = payload[1];
    message->front_rpm = read_i16_le(&payload[2]);
    message->center_rpm = read_i16_le(&payload[4]);
    message->rear_rpm = read_i16_le(&payload[6]);
    return true;
}

bool ugv_can_encode_fault_report(uint8_t *payload, size_t size,
                                 const ugv_can_fault_report_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_FAULT_LEFT_DLC)) {
        return false;
    }
    payload[0] = message->sequence;
    payload[1] = message->safety_state;
    write_u16_le(&payload[2], message->front_faults);
    write_u16_le(&payload[4], message->center_faults);
    write_u16_le(&payload[6], message->rear_faults);
    return true;
}

bool ugv_can_decode_fault_report(ugv_can_fault_report_t *message,
                                 const uint8_t *payload, size_t size)
{
    if (!can_decode(message, payload, size, UGV_CAN_FAULT_LEFT_DLC)) {
        return false;
    }
    message->sequence = payload[0];
    message->safety_state = payload[1];
    message->front_faults = read_u16_le(&payload[2]);
    message->center_faults = read_u16_le(&payload[4]);
    message->rear_faults = read_u16_le(&payload[6]);
    return true;
}

bool ugv_can_encode_heartbeat(uint8_t *payload, size_t size,
                              const ugv_can_heartbeat_t *message)
{
    if (!can_encode(payload, size, message, UGV_CAN_HEARTBEAT_LEFT_DLC)) {
        return false;
    }
    payload[0] = message->protocol_major;
    payload[1] = message->protocol_minor;
    payload[2] = message->safety_state;
    payload[3] = message->status_flags;
    write_u32_le(&payload[4], message->uptime_ms);
    return true;
}

bool ugv_can_decode_heartbeat(ugv_can_heartbeat_t *message,
                              const uint8_t *payload, size_t size)
{
    if (!can_decode(message, payload, size, UGV_CAN_HEARTBEAT_LEFT_DLC)) {
        return false;
    }
    message->protocol_major = payload[0];
    message->protocol_minor = payload[1];
    message->safety_state = payload[2];
    message->status_flags = payload[3];
    message->uptime_ms = read_u32_le(&payload[4]);
    return true;
}
