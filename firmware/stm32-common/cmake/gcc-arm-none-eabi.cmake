set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)

# Some default GCC settings
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

file(GLOB UGV_STM32_TOOLCHAIN_HINTS
    "$ENV{LOCALAPPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin")
find_program(UGV_ARM_GCC NAMES ${TOOLCHAIN_PREFIX}gcc
    HINTS ${UGV_STM32_TOOLCHAIN_HINTS}
    REQUIRED)
get_filename_component(UGV_ARM_TOOLCHAIN_BIN "${UGV_ARM_GCC}" DIRECTORY)

set(CMAKE_C_COMPILER                "${UGV_ARM_GCC}")
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_LINKER                    "${UGV_ARM_GCC}")
find_program(UGV_ARM_OBJCOPY NAMES ${TOOLCHAIN_PREFIX}objcopy
    HINTS "${UGV_ARM_TOOLCHAIN_BIN}"
    REQUIRED)
find_program(UGV_ARM_SIZE NAMES ${TOOLCHAIN_PREFIX}size
    HINTS "${UGV_ARM_TOOLCHAIN_BIN}"
    REQUIRED)
set(CMAKE_OBJCOPY "${UGV_ARM_OBJCOPY}" CACHE FILEPATH "" FORCE)
set(CMAKE_SIZE "${UGV_ARM_SIZE}" CACHE FILEPATH "" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")

# The cyclomatic-complexity parameter must be defined for the Cyclomatic complexity feature in STM32CubeIDE to work.
# However, most GCC toolchains do not support this option, which causes a compilation error; for this reason, the feature is disabled by default.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcyclomatic-complexity")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(STM32_COMMON_DIR "${CMAKE_SOURCE_DIR}/firmware/stm32-common")
set(UGV_LINKER_LAYOUT "STANDALONE" CACHE STRING
    "Flash layout: STANDALONE, OTA_APP, or BOOTLOADER")
set_property(CACHE UGV_LINKER_LAYOUT PROPERTY STRINGS STANDALONE OTA_APP BOOTLOADER)
if(UGV_LINKER_LAYOUT STREQUAL "OTA_APP")
    set(UGV_LINKER_SCRIPT "${STM32_COMMON_DIR}/STM32G431XX_OTA_APP.ld")
elseif(UGV_LINKER_LAYOUT STREQUAL "BOOTLOADER")
    set(UGV_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/firmware/stm32-bootloader/STM32G431XX_BOOTLOADER.ld")
elseif(UGV_LINKER_LAYOUT STREQUAL "STANDALONE")
    set(UGV_LINKER_SCRIPT "${STM32_COMMON_DIR}/STM32G431XX_FLASH.ld")
else()
    message(FATAL_ERROR "UGV_LINKER_LAYOUT must be STANDALONE, OTA_APP, or BOOTLOADER")
endif()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${UGV_LINKER_SCRIPT}\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
if(NOT UGV_LINKER_LAYOUT STREQUAL "BOOTLOADER")
    # nano.specs omits float printf/scanf support unless pulled explicitly.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _scanf_float")
endif()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
