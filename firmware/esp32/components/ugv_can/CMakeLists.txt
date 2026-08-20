set(UGV_CAN_SHARED_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../shared/can")

idf_component_register(
    SRCS "${UGV_CAN_SHARED_DIR}/ugv_can_codec.c"
    INCLUDE_DIRS "${UGV_CAN_SHARED_DIR}"
)
