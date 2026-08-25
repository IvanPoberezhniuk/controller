#ifndef UGV_STM32_NODE_COMMON_CONFIG_H
#define UGV_STM32_NODE_COMMON_CONFIG_H

/* Set to 1 together with the documented manual CubeMX migration:
 * PA11/PA12 -> FDCAN and PB8 -> TIM16_CH1 center LPWM. Keeping this 0 lets
 * the legacy generated .ioc continue to build meanwhile. */
#ifndef UGV_FINAL_OTA_PINOUT_CONFIGURED
#define UGV_FINAL_OTA_PINOUT_CONFIGURED 0
#endif

/* Keep amperes invalid until the six physical R_IS/L_IS inputs are protected,
 * wired, and the scale in configuration.c is calibrated on the real driver. */
#ifndef UGV_CURRENT_SENSE_CALIBRATED
#define UGV_CURRENT_SENSE_CALIBRATED 0
#endif

#if UGV_CURRENT_SENSE_CALIBRATED != 0 && UGV_CURRENT_SENSE_CALIBRATED != 1
#error "UGV_CURRENT_SENSE_CALIBRATED must be 0 or 1"
#endif

#endif /* UGV_STM32_NODE_COMMON_CONFIG_H */
