#ifndef UGV_STM32_NODE_COMMON_CONFIG_H
#define UGV_STM32_NODE_COMMON_CONFIG_H

/* Set to 1 together with the documented manual CubeMX migration:
 * PA11/PA12 -> FDCAN and PB8 -> TIM16_CH1 center LPWM. Keeping this 0 lets
 * the legacy generated .ioc continue to build meanwhile. */
#ifndef UGV_FINAL_OTA_PINOUT_CONFIGURED
#define UGV_FINAL_OTA_PINOUT_CONFIGURED 0
#endif

#endif /* UGV_STM32_NODE_COMMON_CONFIG_H */
