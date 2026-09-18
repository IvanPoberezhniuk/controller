#include <stdint.h>

#include "stm32g4xx_hal.h"
#include "ugv_boot_flash.h"
#include "ugv_boot_request_stm32.h"
#include "ugv_boot_uart.h"
#include "ugv_bootloader.h"
#include "ugv_fw_update_protocol.h"
#include "ugv_uart_protocol.h"

#ifndef UGV_BOOT_NODE_ID
#error "UGV_BOOT_NODE_ID must identify the LEFT or RIGHT motor node"
#endif

static void force_motor_outputs_safe(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Hold all PWM outputs and the three common driver-enable nets low while
     * the application is absent. PA4-PA7 and PB2/PB12 are current-sense ADC
     * inputs and must never be driven by the bootloader. External pull-downs
     * remain mandatory for reset/power-up. */
    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                          GPIO_PIN_14 | GPIO_PIN_15,
                      GPIO_PIN_RESET);

    GPIO_InitTypeDef outputs = {
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_PULLDOWN,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    outputs.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &outputs);
    outputs.Pin = GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                  GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &outputs);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

int main(void)
{
    HAL_Init();
    force_motor_outputs_safe();

    const bool boot_requested = ugv_boot_request_consume();
    if (!boot_requested &&
        ugv_boot_flash_application_valid((uint8_t)UGV_BOOT_NODE_ID)) {
        ugv_boot_flash_jump_to_application();
    }

    if (!ugv_boot_uart_init()) {
        for (;;) {
            IWDG->KR = 0xaaaau;
        }
    }

    ugv_boot_flash_context_t flash_context;
    ugv_boot_flash_init(&flash_context, (uint8_t)UGV_BOOT_NODE_ID);
    const ugv_bootloader_platform_t platform =
        ugv_boot_flash_platform(&flash_context);
    ugv_bootloader_t bootloader;
    ugv_bootloader_init(&bootloader, (uint8_t)UGV_BOOT_NODE_ID, &platform);

    for (;;) {
        IWDG->KR = 0xaaaau;

        ugv_uart_frame_t frame;
        if (!ugv_boot_uart_receive(&frame)) {
            continue;
        }

        ugv_fw_status_t status;
        bool send_status = false;
        if (frame.type == UGV_UART_MSG_FW_COMMAND) {
            ugv_fw_command_t command;
            if (ugv_fw_decode_command(&command, frame.payload,
                                      frame.payload_size)) {
                send_status = ugv_bootloader_handle_command(
                    &bootloader, &command, &status);
            }
        } else if (frame.type == UGV_UART_MSG_FW_DATA) {
            ugv_fw_data_t data;
            if (ugv_fw_decode_data(&data, frame.payload,
                                   frame.payload_size)) {
                send_status = ugv_bootloader_handle_data(
                    &bootloader, &data, &status);
            }
        }

        if (send_status) {
            (void)ugv_boot_uart_send_status(&status);
        }
    }
}
