idf_component_register(
    SRCS "main.c" "stm32_uart_updater.c"
    INCLUDE_DIRS "."
    REQUIRES ugv_board ugv_crsf ugv_manual_control ugv_serial ugv_update
             ugv_bms_ble esp_driver_gpio esp_driver_uart esp_timer
)
