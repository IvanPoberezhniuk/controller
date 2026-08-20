#ifndef UGV_STM32_NODE_COMMON_CONFIG_H
#define UGV_STM32_NODE_COMMON_CONFIG_H

/* Both motor nodes use the same local CD74HC4067 wiring. Keep sensing
 * disabled until the shared CubeMX project assigns S0-S3 and MUX_SIG. */
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

#endif /* UGV_STM32_NODE_COMMON_CONFIG_H */
