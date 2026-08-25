#ifndef UGV_STM32_LEFT_NODE_CONFIG_H
#define UGV_STM32_LEFT_NODE_CONFIG_H

#include "ugv_can_protocol.h"
#include "node_common_config.h"

#define UGV_NODE_ROLE_NAME       "left"
#define UGV_NODE_CAN_ID          UGV_CAN_NODE_LEFT
#define UGV_NODE_TELEMETRY_ID    UGV_CAN_MSG_TELEMETRY_LEFT
#define UGV_NODE_FAULT_ID        UGV_CAN_MSG_FAULT_LEFT

/* Verify signs on the assembled drivetrain before enabling closed-loop motion. */
#define UGV_MOTOR_FRONT_DIRECTION  (+1)
#define UGV_MOTOR_CENTER_DIRECTION (+1)
#define UGV_MOTOR_REAR_DIRECTION   (+1)

#define UGV_ENCODER_FRONT_DIRECTION  (-1)
#define UGV_ENCODER_CENTER_DIRECTION (+1)
#define UGV_ENCODER_REAR_DIRECTION   (+1)

#endif /* UGV_STM32_LEFT_NODE_CONFIG_H */
