idf_component_register(
    SRCS "ugv_bms_ble.c"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_timer
)
