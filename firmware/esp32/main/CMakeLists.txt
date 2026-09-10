idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ugv_board ugv_can ugv_crsf ugv_manual_control
             esp_driver_uart esp_driver_twai esp_timer
)
