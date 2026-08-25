# STM32 Left motor node

Target-specific configuration for the left STM32G431CBT6. The executable is
built from `firmware/stm32-common` with the `stm32-left-*` CMake presets.

This node controls the front-left, center-left, and rear-left motors and owns
its local CD74HC4067 current-sense multiplexer. Channels 6-15 are reserved and
remain unconnected in the current build.
