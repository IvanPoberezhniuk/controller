#ifndef PROTOCOL_CAN_PROTOCOL_H
#define PROTOCOL_CAN_PROTOCOL_H

#include <stdint.h>

/*
 * Single authoritative definition of the UGV CAN protocol, per
 * .claude/skills/ugv-can-protocol/SKILL.md. Data-only: no FDCAN peripheral
 * calls here. FDCAN1 is not configured on the STM32 yet (added in the
 * "add CAN" roadmap milestone) -- this header exists now so firmware,
 * Raspberry Pi service, and PC tooling can share one definition from day one.
 */

typedef enum {
    UGV_CAN_NODE_LEFT           = 0x10,
    UGV_CAN_NODE_RIGHT          = 0x11,
    UGV_CAN_NODE_PI_GATEWAY     = 0x20,
    UGV_CAN_NODE_BMS_GATEWAY    = 0x30, /* future */
    UGV_CAN_NODE_LIGHTING       = 0x40, /* future */
} ugv_can_node_id_t;

typedef enum {
    UGV_CAN_MSG_VEHICLE_MOTION  = 0x100,
    UGV_CAN_MSG_WHEEL_TARGETS   = 0x101,
    UGV_CAN_MSG_SYSTEM_ENABLE   = 0x110,
    UGV_CAN_MSG_AUX_LIGHTING    = 0x120,
    UGV_CAN_MSG_TELEMETRY_LEFT  = 0x180,
    UGV_CAN_MSG_TELEMETRY_RIGHT = 0x181,
    UGV_CAN_MSG_FAULT_LEFT      = 0x190,
    UGV_CAN_MSG_FAULT_RIGHT     = 0x191,
    UGV_CAN_MSG_TEMPS_LEFT      = 0x1A0,
    UGV_CAN_MSG_TEMPS_RIGHT     = 0x1A1,
    UGV_CAN_MSG_HEARTBEAT_BASE  = 0x700, /* + node ID */
} ugv_can_message_id_t;

/* 0x100 -- vehicle motion command, Raspberry Pi -> motor node, 8 bytes */
typedef struct {
    uint8_t  sequence;
    uint8_t  mode_flags;
    int16_t  left_target;   /* signed, units TBD (raw PWM or RPM*scale) */
    int16_t  right_target;
    uint8_t  limit;         /* speed/torque limit */
    uint8_t  reserved;
} ugv_can_motion_cmd_t;

/* 0x110 -- system enable / emergency stop, 1+ bytes */
typedef struct {
    uint8_t enabled;
    uint8_t emergency_stop;
} ugv_can_system_enable_t;

/* Heartbeat and command-timeout thresholds, per ugv-can-protocol SKILL.md.
 * The STM32 must not continue using the last speed command indefinitely. */
#define UGV_CAN_COMMAND_PERIOD_MS_MIN     10u   /* 100 Hz */
#define UGV_CAN_COMMAND_PERIOD_MS_MAX     20u   /* 50 Hz */
#define UGV_CAN_COMMAND_WARN_MS           150u
#define UGV_CAN_COMMAND_CONTROLLED_STOP_MS 250u
#define UGV_CAN_COMMAND_DRIVER_DISABLE_MS 500u

#define UGV_CAN_BITRATE_BPS 500000u

#endif /* PROTOCOL_CAN_PROTOCOL_H */
