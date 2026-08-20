#ifndef UGV_CAN_PROTOCOL_H
#define UGV_CAN_PROTOCOL_H

#include <stdint.h>

/*
 * Platform-neutral Classic CAN wire contract. Multi-byte values are
 * little-endian and must be serialized with ugv_can_codec; never memcpy a C
 * struct into a frame. Tests/can/test_dbc_sync.ps1 verifies that ugv.dbc has
 * the same message IDs and DLCs as UGV_CAN_MESSAGE_TABLE.
 */

#define UGV_CAN_PROTOCOL_VERSION_MAJOR 1u
#define UGV_CAN_PROTOCOL_VERSION_MINOR 2u

#define UGV_CAN_BITRATE_BPS             500000u
#define UGV_CAN_COMMAND_PERIOD_MS_MIN   10u
#define UGV_CAN_COMMAND_PERIOD_MS_MAX   20u
#define UGV_CAN_COMMAND_TIMEOUT_MS      300u
#define UGV_CAN_RC_LINK_TIMEOUT_MS      100u
#define UGV_CAN_AUTO_REQUEST_TIMEOUT_MS 300u

typedef enum {
    UGV_CAN_NODE_LEFT        = 0x10,
    UGV_CAN_NODE_RIGHT       = 0x11,
    UGV_CAN_NODE_PI_GATEWAY  = 0x20,
    UGV_CAN_NODE_BMS_GATEWAY = 0x30, /* reserved */
    UGV_CAN_NODE_ESP32_AUX   = 0x40,
} ugv_can_node_id_t;

/* symbol, DBC message name, standard 11-bit CAN ID, DLC */
#define UGV_CAN_MESSAGE_TABLE(X) \
    X(VEHICLE_MOTION,  VehicleMotion,  0x100, 8u) \
    X(AUTO_MOTION_REQUEST, AutoMotionRequest, 0x101, 8u) \
    X(SYSTEM_ENABLE,   SystemEnable,   0x110, 2u) \
    X(AUTO_ENABLE_REQUEST, AutoEnableRequest, 0x111, 2u) \
    X(AUX_LIGHTING,    AuxLighting,    0x120, 4u) \
    X(CONTROL_STATUS,  ControlStatus,  0x130, 8u) \
    X(TELEMETRY_LEFT,  TelemetryLeft,  0x180, 8u) \
    X(TELEMETRY_RIGHT, TelemetryRight, 0x181, 8u) \
    X(FAULT_LEFT,      FaultLeft,      0x190, 8u) \
    X(FAULT_RIGHT,     FaultRight,     0x191, 8u) \
    X(TEMPS_LEFT,      TempsLeft,      0x1A0, 8u) \
    X(TEMPS_RIGHT,     TempsRight,     0x1A1, 8u) \
    X(HEARTBEAT_LEFT,  HeartbeatLeft,  0x710, 8u) \
    X(HEARTBEAT_RIGHT, HeartbeatRight, 0x711, 8u) \
    X(HEARTBEAT_PI,    HeartbeatPi,    0x720, 8u) \
    X(HEARTBEAT_ESP32, HeartbeatEsp32, 0x740, 8u)

typedef enum {
#define UGV_CAN_DECLARE_ID(symbol, dbc_name, id, dlc) UGV_CAN_MSG_##symbol = id,
    UGV_CAN_MESSAGE_TABLE(UGV_CAN_DECLARE_ID)
#undef UGV_CAN_DECLARE_ID
} ugv_can_message_id_t;

enum {
#define UGV_CAN_DECLARE_DLC(symbol, dbc_name, id, dlc) UGV_CAN_##symbol##_DLC = dlc,
    UGV_CAN_MESSAGE_TABLE(UGV_CAN_DECLARE_DLC)
#undef UGV_CAN_DECLARE_DLC
};

/*
 * 0x100 ESP32 -> both motor nodes (final command), and
 * 0x101 Raspberry Pi -> ESP32 (AUTO request).
 * Both use the same signed-RPM payload. Only the ESP32 may produce 0x100.
 */
typedef struct {
    uint8_t sequence;
    uint8_t mode_flags;
    int16_t left_target_rpm;
    int16_t right_target_rpm;
    uint8_t limit_pct;
    uint8_t reserved;
} ugv_can_motion_cmd_t;

/*
 * 0x110 ESP32 -> motor nodes (final state), and
 * 0x111 Raspberry Pi -> ESP32 (AUTO request).
 * Both use the same payload. Only the ESP32 may produce 0x110.
 */
typedef struct {
    uint8_t enabled;
    uint8_t emergency_stop;
} ugv_can_system_enable_t;

/* 0x120, Raspberry Pi -> ESP32 AUX. Intensities are 0..100 percent. */
typedef struct {
    uint8_t front_pct;
    uint8_t rear_pct;
    uint8_t left_indicator;
    uint8_t right_indicator;
} ugv_can_aux_lighting_t;

typedef enum {
    UGV_CAN_CONTROL_MODE_DISABLED = 0,
    UGV_CAN_CONTROL_MODE_MANUAL   = 1,
    UGV_CAN_CONTROL_MODE_AUTO     = 2,
} ugv_can_control_mode_t;

typedef enum {
    UGV_CAN_CONTROL_SOURCE_NONE = 0,
    UGV_CAN_CONTROL_SOURCE_RC   = 1,
    UGV_CAN_CONTROL_SOURCE_PI   = 2,
} ugv_can_control_source_t;

/* 0x130, ESP32 -> Raspberry Pi. CRSF link and command-arbiter status. */
typedef struct {
    uint8_t sequence;
    uint8_t selected_mode;
    uint8_t active_source;
    uint8_t enabled;
    uint8_t rc_link_up;
    uint8_t rc_failsafe;
    uint8_t link_quality_pct;
    int8_t rssi_dbm;
} ugv_can_control_status_t;

/* 0x180/0x181, motor node -> Raspberry Pi/ESP32. */
typedef struct {
    uint8_t sequence;
    uint8_t safety_state;
    int16_t front_rpm;
    int16_t center_rpm;
    int16_t rear_rpm;
} ugv_can_motor_telemetry_t;

typedef enum {
    UGV_CAN_MOTOR_FAULT_STALLED         = 1u << 0,
    UGV_CAN_MOTOR_FAULT_OVERCURRENT     = 1u << 1,
    UGV_CAN_MOTOR_FAULT_OVERTEMPERATURE = 1u << 2,
    UGV_CAN_MOTOR_FAULT_DRIVER          = 1u << 3,
    UGV_CAN_MOTOR_FAULT_ENCODER         = 1u << 4,
} ugv_can_motor_fault_flag_t;

/* 0x190/0x191, one 16-bit fault mask per local motor. */
typedef struct {
    uint8_t sequence;
    uint8_t safety_state;
    uint16_t front_faults;
    uint16_t center_faults;
    uint16_t rear_faults;
} ugv_can_fault_report_t;

/* 0x1A0/0x1A1. Temperatures use signed centi-degrees Celsius. */
typedef struct {
    uint8_t sequence;
    uint8_t valid_mask; /* bits 0..2: front, center, rear */
    int16_t front_centi_c;
    int16_t center_centi_c;
    int16_t rear_centi_c;
} ugv_can_temperatures_t;

typedef struct {
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint8_t safety_state;
    uint8_t status_flags;
    uint32_t uptime_ms;
} ugv_can_heartbeat_t;

#endif /* UGV_CAN_PROTOCOL_H */
