add_executable(${CMAKE_PROJECT_NAME})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(STM32_COMMON_DIR "${CMAKE_SOURCE_DIR}/firmware/stm32-common")
set(STM32_HAL_DIR "${STM32_COMMON_DIR}/Drivers/STM32G4xx_HAL_Driver")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ugv_bootloader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Src/ugv_boot_can.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Src/ugv_boot_flash.c
    ${STM32_COMMON_DIR}/startup_stm32g431xx.s
    ${STM32_COMMON_DIR}/Core/Src/system_stm32g4xx.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_cortex.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_fdcan.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_flash.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_flash_ex.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_flash_ramfunc.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_gpio.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_rcc.c
    ${STM32_HAL_DIR}/Src/stm32g4xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/shared/update/ugv_boot_request_stm32.c
    ${CMAKE_SOURCE_DIR}/shared/update/ugv_crc32.c
    ${CMAKE_SOURCE_DIR}/shared/update/ugv_fw_update_protocol.c
    ${CMAKE_SOURCE_DIR}/shared/update/ugv_image_metadata.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Inc
    ${STM32_COMMON_DIR}/Core/Inc
    ${STM32_HAL_DIR}/Inc
    ${STM32_HAL_DIR}/Inc/Legacy
    ${STM32_COMMON_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${STM32_COMMON_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/shared/can
    ${CMAKE_SOURCE_DIR}/shared/update
)

if(UGV_NODE_ROLE_UPPER STREQUAL "LEFT")
    set(UGV_BOOT_NODE_ID_VALUE 0x10U)
else()
    set(UGV_BOOT_NODE_ID_VALUE 0x11U)
endif()

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32G431xx
    HAL_FDCAN_MODULE_ENABLED
    UGV_BOOT_NODE_ID=${UGV_BOOT_NODE_ID_VALUE}
)
target_link_libraries(${CMAKE_PROJECT_NAME} m)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating raw bootloader image ${CMAKE_PROJECT_NAME}.bin"
)
