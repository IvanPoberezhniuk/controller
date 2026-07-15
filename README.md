# UGV Controller Firmware (STM32G431CBT6)

Motor-control node firmware for one side of the 6x6 UGV — see `CLAUDE.md`
and `.claude/skills/` for the full project architecture, hardware
decisions, and roadmap. This node drives three BTS7960-based brushed-DC
motors (front/center/rear) with quadrature encoder feedback.

## Status: Milestone 1 (bring-up)

Per the roadmap in `CLAUDE.md`, this is the "control one motor" stage —
hardware/pin configuration covers all three motors, but only `motor0`
(front) is exercised so far. No CAN yet (added in a later milestone);
motor commands and telemetry go over the debug USART2 console instead.

## Layout

```
Core/, Drivers/          STM32CubeMX-generated (HAL, CMSIS, startup, MX_*_Init)
Application/              Control algorithms: motor_control, encoder,
                           current_monitor, fault_manager, safety, configuration,
                           app_main (ties it together, called from Core/Src/main.c)
Platform/                 Hardware adapters not owned by CubeMX: board (reset
                           reason), timebase (TIM6-paced control loop), watchdog
Protocol/                 can_protocol.h -- data-only CAN message/ID definitions,
                           shared reference for when FDCAN is wired up later
Tests/                    Host-buildable (non-ARM) tests for the pure math in
                           Application/Src/motor_math.c
UGV_Controllers.ioc        CubeMX project file -- edit pin/clock/peripheral
                           config here, then regenerate
```

## Building

Uses the STM32Cube-for-VS-Code bundle manager's toolchain (arm-none-eabi-gcc,
ninja, cmake) via `cmake --preset Debug && cmake --build build/Debug`, or the
VS Code CMake Tools integration.

Node role (`LEFT`/`RIGHT`, both nodes run identical source) is a CMake
option: `-DUGV_NODE_ROLE=RIGHT` (defaults to `LEFT`).

## Debug console (USART2, 115200 8N1)

Commands: `arm`, `estop`, `clear`, `m<0|1|2> <rpm>` (e.g. `m0 120`).
Telemetry line printed ~4 Hz.

## Known placeholders (see `.claude/skills/ugv-project-plan`)

- `Application/Inc/configuration.h` tunables (PID gains, encoder counts/rev,
  current-sense scale, stall threshold) are conservative bench defaults, not
  measured values.
- Control loop is polled from TIM6's update flag, not interrupt-driven (its
  NVIC interrupt isn't enabled in the current `.ioc`).
- `PA10` still carries the CubeMX macro name `MOTOR2_R_EN_Pin` in
  `Core/Inc/main.h` even though that pin is actually `TIM1_CH3` (motor1
  RPWM) -- a stale label from an earlier pin reassignment. The real motor2
  R_EN output is `PB10`, which has no CubeMX label at all. `Platform/Inc/board.h`
  defines `MOTOR2_R_EN_REAL_Pin`/`_GPIO_Port` for the correct pin; don't use
  `MOTOR2_R_EN_Pin` from `main.h` for GPIO control.

## Running host-side tests

```
gcc -std=c11 -Wall -Wextra -IApplication/Inc \
    Tests/test_motor_control.c Application/Src/motor_math.c -lm \
    -o test_motor_control && ./test_motor_control
```
