#ifndef UGV_STM32_RIGHT_NODE_CONFIG_H
#define UGV_STM32_RIGHT_NODE_CONFIG_H

#include "ugv_can_protocol.h"
#include "node_common_config.h"

#define UGV_NODE_ROLE_NAME       "right"
#define UGV_NODE_CAN_ID          UGV_CAN_NODE_RIGHT
#define UGV_NODE_TELEMETRY_ID    UGV_CAN_MSG_TELEMETRY_RIGHT
#define UGV_NODE_FAULT_ID        UGV_CAN_MSG_FAULT_RIGHT
#define UGV_NODE_TEMPERATURES_ID UGV_CAN_MSG_TEMPS_RIGHT

/* Keep neutral defaults until motor and encoder polarity is measured. */
#define UGV_MOTOR_FRONT_DIRECTION  (+1)
#define UGV_MOTOR_CENTER_DIRECTION (+1)
#define UGV_MOTOR_REAR_DIRECTION   (+1)

#define UGV_ENCODER_FRONT_DIRECTION  (+1)
#define UGV_ENCODER_CENTER_DIRECTION (+1)
#define UGV_ENCODER_REAR_DIRECTION   (+1)


#endif /* UGV_STM32_RIGHT_NODE_CONFIG_H */
