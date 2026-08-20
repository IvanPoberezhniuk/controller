#ifndef UGV_ESP32_BOARD_H
#define UGV_ESP32_BOARD_H

#include "driver/gpio.h"

typedef struct {
    gpio_num_t can_tx;
    gpio_num_t can_rx;
    gpio_num_t oled_sda;
    gpio_num_t oled_scl;
    gpio_num_t encoder_a;
    gpio_num_t encoder_b;
    gpio_num_t encoder_button;
    gpio_num_t sensor_sda;
    gpio_num_t sensor_scl;
    gpio_num_t imu_interrupt;
    gpio_num_t gps_tx;
    gpio_num_t gps_rx;
    gpio_num_t front_light;
    gpio_num_t rear_light;
    gpio_num_t left_indicator;
    gpio_num_t right_indicator;
    gpio_num_t buzzer;
    gpio_num_t service_led;
    gpio_num_t status_rgb;
} ugv_esp32_board_config_t;

const ugv_esp32_board_config_t *ugv_esp32_board_config(void);

#endif /* UGV_ESP32_BOARD_H */
