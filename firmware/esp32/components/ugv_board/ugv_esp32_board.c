#include "ugv_esp32_board.h"

/* Sixspan ESP32-S3-N16R8. GPIO19/20 remain assigned to native USB and
 * GPIO43/44 remain the ROM programming UART. At runtime UART0 is routed to
 * the left motor node and UART2 to the right motor node. */
static const ugv_esp32_board_config_t s_board = {
    .left_uart_tx = GPIO_NUM_39,
    .left_uart_rx = GPIO_NUM_40,
    .right_uart_tx = GPIO_NUM_41,
    .right_uart_rx = GPIO_NUM_42,
    .oled_sda = GPIO_NUM_8,
    .oled_scl = GPIO_NUM_9,
    .encoder_a = GPIO_NUM_4,
    .encoder_b = GPIO_NUM_5,
    .encoder_button = GPIO_NUM_6,
    .sensor_sda = GPIO_NUM_1,
    .sensor_scl = GPIO_NUM_2,
    .imu_interrupt = GPIO_NUM_7,
    .gps_tx = GPIO_NUM_15,
    .gps_rx = GPIO_NUM_16,
    .crsf_tx = GPIO_NUM_38,
    .crsf_rx = GPIO_NUM_21,
    .front_light = GPIO_NUM_10,
    .rear_light = GPIO_NUM_11,
    .left_indicator = GPIO_NUM_12,
    .right_indicator = GPIO_NUM_13,
    .buzzer = GPIO_NUM_14,
    .service_led = GPIO_NUM_47,
    .status_rgb = GPIO_NUM_48,
};

const ugv_esp32_board_config_t *ugv_esp32_board_config(void)
{
    return &s_board;
}
