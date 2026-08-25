#ifndef PLATFORM_BOARD_H
#define PLATFORM_BOARD_H

#include "main.h"
#include "node_common_config.h"

/* CubeMX declares these HAL handles as globals in Core/Src/main.c but only
 * within that translation unit's own scope; this header is the single
 * place the rest of the firmware pulls them from via extern. */
extern TIM_HandleTypeDef  htim1;
extern TIM_HandleTypeDef  htim2;
extern TIM_HandleTypeDef  htim3;
extern TIM_HandleTypeDef  htim4;
extern TIM_HandleTypeDef  htim6;
extern TIM_HandleTypeDef  htim15;
#if UGV_FINAL_OTA_PINOUT_CONFIGURED
extern TIM_HandleTypeDef  htim16;
#endif
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef  hadc1;
extern ADC_HandleTypeDef  hadc2;
extern IWDG_HandleTypeDef hiwdg;

/* Each dual-half-bridge driver uses one common enable net: its R_EN and L_EN
 * inputs are wired together and driven from this GPIO. Explicit definitions
 * keep application and bootloader pin ownership independent of stale CubeMX
 * labels until the user regenerates the final .ioc manually. */
#define MOTOR0_COMMON_EN_GPIO_Port GPIOB
#define MOTOR0_COMMON_EN_Pin       GPIO_PIN_0
#define MOTOR1_COMMON_EN_GPIO_Port GPIOB
#define MOTOR1_COMMON_EN_Pin       GPIO_PIN_9
#define MOTOR2_COMMON_EN_GPIO_Port GPIOB
#define MOTOR2_COMMON_EN_Pin       GPIO_PIN_10

/* Reset reason, decoded once at boot before HAL clears the RCC reset flags so
 * telemetry can report why the node restarted. */
typedef enum {
    BOARD_RESET_UNKNOWN = 0,
    BOARD_RESET_POWER_ON, /* brown-out reset -- STM32G4 has no separate POR flag */
    BOARD_RESET_PIN,
    BOARD_RESET_SOFTWARE,
    BOARD_RESET_IWDG,
    BOARD_RESET_WWDG,
    BOARD_RESET_LOW_POWER,
} board_reset_reason_t;

void board_init(void);
board_reset_reason_t board_get_reset_reason(void);
const char *board_reset_reason_name(board_reset_reason_t reason);

#endif /* PLATFORM_BOARD_H */
