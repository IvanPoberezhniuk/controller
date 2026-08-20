# UGV controller monorepo

Firmware and protocol definitions for the 6x6 UGV. The vehicle has two
STM32G431CBT6 motor-control nodes, one ESP32-S3 auxiliary/UI node, and a
Raspberry Pi 5 as the high-level computer and CAN coordinator.

## Nodes

| Node | Responsibilities |
| --- | --- |
| STM32 Left | Front-left, center-left, rear-left motors; encoders; six R_IS/L_IS signals; three motor temperatures |
| STM32 Right | Front-right, center-right, rear-right motors; encoders; six R_IS/L_IS signals; three motor temperatures |
| ESP32 AUX | OLED, encoder UI, IMU, ambient light, GPS, vehicle lighting, buzzer, CAN/TWAI |
| Raspberry Pi 5 | Camera, full audio, navigation, high-level control, logging and CAN coordination |

Each STM32 owns one CD74HC4067. Both STM32 targets compile the same motor
firmware and select only their role-specific configuration at build time.

## Repository layout

```text
firmware/
  stm32-common/  shared CubeMX/HAL and motor-control implementation
  stm32-left/    left node identity and calibration signs
  stm32-right/   right node identity and calibration signs
  esp32/         independent ESP-IDF project for Sixspan ESP32-S3-N16R8
shared/can/      protocol IDs, payload types, explicit codec and DBC
Tests/           host-side STM32 math and CAN codec tests
docs/            architecture, wiring, pinouts and CubeMX notes
tools/           build, flash and serial-console PowerShell scripts
```

`F:\work\academy\blinkESP32` is not part of this repository and remains an
untouched reference for the proven SH1106 OLED setup.

## STM32 builds

From the repository root:

```powershell
cmake --preset stm32-left-debug
cmake --build --preset stm32-left-debug

cmake --preset stm32-right-debug
cmake --build --preset stm32-right-debug
```

Images are written to:

- `build/stm32-left-debug/UGV_STM32_LEFT.elf`
- `build/stm32-right-debug/UGV_STM32_RIGHT.elf`

Flash with `tools/flash-left.ps1` or `tools/flash-right.ps1`.

## ESP32 build

From an initialized ESP-IDF shell:

```powershell
idf.py -C firmware/esp32 set-target esp32s3
idf.py -C firmware/esp32 build
```

The ESP32 project currently provides the board definition, shared CAN codec,
16 MB flash/8 MB octal PSRAM defaults, and a minimal bring-up application.
Peripheral drivers are added independently after hardware validation.

## Current bring-up status

The STM32 motor firmware is still at motor-node bring-up stage. FDCAN is not
yet enabled in CubeMX. Current monitoring is being migrated to the local
CD74HC4067 and its final GPIO/ADC pin assignment still has to be committed to
the `.ioc` before that module can build and run on hardware.

See [architecture](docs/architecture.md), [CAN protocol](docs/can-protocol.md),
and the node pinout documents under `docs/`.
