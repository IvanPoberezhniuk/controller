# STM32 Right motor node

Target-specific configuration for the right STM32G431CBT6. The executable is
built from `firmware/stm32-common` with the `stm32-right-*` CMake presets.

This node controls the front-right, center-right, and rear-right motors and
owns its local CD74HC4067 current/temperature multiplexer.
