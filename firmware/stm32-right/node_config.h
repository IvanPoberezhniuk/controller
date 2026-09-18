#ifndef UGV_STM32_RIGHT_NODE_CONFIG_H
#define UGV_STM32_RIGHT_NODE_CONFIG_H

#include "node_common_config.h"
#include "ugv_uart_protocol.h"

#define UGV_NODE_ROLE_NAME       "right"
#define UGV_NODE_ROLE_CODE       UGV_NODE_CODE_RIGHT

/* Keep neutral defaults until motor and encoder polarity is measured. */
#define UGV_MOTOR_FRONT_DIRECTION  (+1)
#define UGV_MOTOR_CENTER_DIRECTION (+1)
#define UGV_MOTOR_REAR_DIRECTION   (+1)

#define UGV_ENCODER_FRONT_DIRECTION  (+1)
#define UGV_ENCODER_CENTER_DIRECTION (+1)
#define UGV_ENCODER_REAR_DIRECTION   (+1)


#endif /* UGV_STM32_RIGHT_NODE_CONFIG_H */
