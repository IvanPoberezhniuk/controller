#ifndef PLATFORM_BOARD_H
#define PLATFORM_BOARD_H

#include "main.h"

/* CubeMX declares these HAL handles as globals in Core/Src/main.c but only
 * within that translation unit's own scope; this header is the single
 * place the rest of the firmware pulls them from via extern. */
extern TIM_HandleTypeDef  htim1;
extern TIM_HandleTypeDef  htim2;
extern TIM_HandleTypeDef  htim3;
extern TIM_HandleTypeDef  htim4;
extern TIM_HandleTypeDef  htim6;
extern TIM_HandleTypeDef  htim15;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef  hadc1;
extern ADC_HandleTypeDef  hadc2;
extern IWDG_HandleTypeDef hiwdg;

/* PA10 still carries the macro name MOTOR2_R_EN_Pin in main.h -- a leftover
 * from before that pin was reassigned to TIM1_CH3 (motor1 RPWM). The real
 * motor2 R_EN output is PB10, which never got a CubeMX User Label, so it has
 * no generated macro at all. Use these two defines everywhere instead of the
 * misleading MOTOR2_R_EN_Pin/MOTOR2_R_EN_GPIO_Port from main.h. */
#define MOTOR2_R_EN_REAL_GPIO_Port GPIOB
#define MOTOR2_R_EN_REAL_Pin       GPIO_PIN_10

/* Reset reason, decoded once at boot in board_init() before HAL clears the
 * RCC reset flags -- safety.c/telemetry need this per
 * ugv-telemetry-safety SKILL.md ("publish reset reason"). */
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
