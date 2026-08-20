#ifndef UGV_STM32_LEFT_NODE_CONFIG_H
#define UGV_STM32_LEFT_NODE_CONFIG_H

#include "ugv_can_protocol.h"

#define UGV_NODE_ROLE_NAME       "left"
#define UGV_NODE_CAN_ID          UGV_CAN_NODE_LEFT
#define UGV_NODE_TELEMETRY_ID    UGV_CAN_MSG_TELEMETRY_LEFT
#define UGV_NODE_FAULT_ID        UGV_CAN_MSG_FAULT_LEFT
#define UGV_NODE_TEMPERATURES_ID UGV_CAN_MSG_TEMPS_LEFT

/* Verify signs on the assembled drivetrain before enabling closed-loop motion. */
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

#endif /* UGV_STM32_LEFT_NODE_CONFIG_H */
