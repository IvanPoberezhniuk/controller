#ifndef UGV_STM32_RIGHT_NODE_CONFIG_H
#define UGV_STM32_RIGHT_NODE_CONFIG_H

#include "ugv_can_protocol.h"

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

/* One CD74HC4067 is local to this node: six current signals + three temps. */
#define UGV_MUX_GPIO_CONFIGURED 0
#define UGV_MUX_CH_MOTOR0_RIS  0u
#define UGV_MUX_CH_MOTOR0_LIS  1u
#define UGV_MUX_CH_MOTOR1_RIS  2u
#define UGV_MUX_CH_MOTOR1_LIS  3u
#define UGV_MUX_CH_MOTOR2_RIS  4u
#define UGV_MUX_CH_MOTOR2_LIS  5u
#define UGV_MUX_CH_MOTOR0_TEMP 6u
#define UGV_MUX_CH_MOTOR1_TEMP 7u
#define UGV_MUX_CH_MOTOR2_TEMP 8u

#endif /* UGV_STM32_RIGHT_NODE_CONFIG_H */
