# Shared STM32G431 motor-node firmware. The repository root selects the
# LEFT/RIGHT target and supplies UGV_NODE_ROLE_* variables.
add_executable(${CMAKE_PROJECT_NAME})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/app_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/configuration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/current_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/fault_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/motor_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/motor_math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/safety.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Src/board.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Src/timebase.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Src/watchdog.c
    ${CMAKE_SOURCE_DIR}/shared/can/ugv_can_codec.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/firmware/stm32-${UGV_NODE_ROLE_LOWER}
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Inc
    ${CMAKE_SOURCE_DIR}/shared/can
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    UGV_NODE_ROLE_${UGV_NODE_ROLE_UPPER}
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
