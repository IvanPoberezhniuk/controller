#include <stdint.h>

#include "stm32g4xx_hal.h"
#include "ugv_boot_can.h"
#include "ugv_boot_flash.h"
#include "ugv_boot_request_stm32.h"
#include "ugv_bootloader.h"
#include "ugv_fw_update_protocol.h"

#ifndef UGV_BOOT_NODE_ID
#error "UGV_BOOT_NODE_ID must identify the LEFT or RIGHT motor node"
#endif

static void force_motor_outputs_safe(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Final OTA pin plan: center enables move to PA4/PA5 and center LPWM moves
     * to PB8/TIM16_CH1, freeing PA11/PA12 for FDCAN. External pull-downs remain
     * mandatory for reset/power-up. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_10 |
                          GPIO_PIN_11,
                      GPIO_PIN_RESET);

    GPIO_InitTypeDef outputs = {
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_PULLDOWN,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    outputs.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &outputs);
    outputs.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_10 |
                  GPIO_PIN_11;
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

    if (!ugv_boot_can_init((uint8_t)UGV_BOOT_NODE_ID)) {
        for (;;) {
            IWDG->KR = 0xaaaau;
        }
    }

    uint16_t data_identifier = 0u;
    (void)ugv_fw_data_id_for_node((uint8_t)UGV_BOOT_NODE_ID,
                                  &data_identifier);

    ugv_boot_flash_context_t flash_context;
    ugv_boot_flash_init(&flash_context, (uint8_t)UGV_BOOT_NODE_ID);
    const ugv_bootloader_platform_t platform =
        ugv_boot_flash_platform(&flash_context);
    ugv_bootloader_t bootloader;
    ugv_bootloader_init(&bootloader, (uint8_t)UGV_BOOT_NODE_ID, &platform);

    for (;;) {
        IWDG->KR = 0xaaaau;

        ugv_boot_can_frame_t frame;
        if (!ugv_boot_can_receive(&frame)) {
            continue;
        }

        ugv_fw_status_t status;
        bool send_status = false;
        if (frame.identifier == UGV_FW_CAN_ID_COMMAND) {
            ugv_fw_command_t command;
            if (ugv_fw_decode_command(&command, frame.payload,
                                      sizeof(frame.payload))) {
                send_status = ugv_bootloader_handle_command(
                    &bootloader, &command, &status);
            }
        } else if (frame.identifier == data_identifier) {
            ugv_fw_data_t data;
            if (ugv_fw_decode_data(&data, frame.payload,
                                   sizeof(frame.payload))) {
                send_status = ugv_bootloader_handle_data(
                    &bootloader, &data, &status);
            }
        }

        if (send_status) {
            (void)ugv_boot_can_send_status((uint8_t)UGV_BOOT_NODE_ID,
                                           &status);
        }
    }
}
