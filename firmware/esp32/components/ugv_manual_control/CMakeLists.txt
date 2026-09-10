idf_component_register(
    SRCS "ugv_manual_control.c"
    INCLUDE_DIRS "include"
    REQUIRES ugv_crsf ugv_can
)
