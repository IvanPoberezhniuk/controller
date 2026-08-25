# UGV controller monorepo

Firmware and protocol definitions for the 6x6 UGV. The vehicle has two
STM32G431CBT6 motor-control nodes, one ESP32-S3 control/AUX node, and a
Raspberry Pi 5 as the high-level computer.

## Nodes

| Node | Responsibilities |
| --- | --- |
| STM32 Left | Front-left, center-left, rear-left motors; encoders; six R_IS/L_IS signals; three motor temperatures |
| STM32 Right | Front-right, center-right, rear-right motors; encoders; six R_IS/L_IS signals; three motor temperatures |
| ESP32 control/AUX | XR4 CRSF receiver, MANUAL/AUTO arbitration, final CAN commands, OLED, encoder UI, QMI8658A IMU, M100-5883 GPS/compass, ambient light, lighting, buzzer |
| Raspberry Pi 5 | Wi-Fi camera/video, full audio, navigation, networking and logging; no direct CAN connection |

Each STM32 owns one CD74HC4067. Both STM32 targets compile the same motor
firmware and select only their role-specific configuration at build time.
The runtime CAN trunk contains only ESP32 and the two STM32 nodes. Raspberry Pi
communicates over Wi-Fi/IP and is not part of the manual-control or motor-safety
path.

## Repository layout

```text
firmware/
  stm32-common/  shared CubeMX/HAL and motor-control implementation
  stm32-bootloader/ role-specific FDCAN recovery bootloader
  stm32-left/    left node identity and calibration signs
  stm32-right/   right node identity and calibration signs
  esp32/         independent ESP-IDF project for Sixspan ESP32-S3-N16R8
shared/can/      protocol IDs, payload types, explicit codec and DBC
shared/update/   OTA protocol, CRC, metadata and flash layout
Tests/           host-side STM32 math and CAN codec tests
docs/            architecture, wiring, pinouts and CubeMX notes
tools/           build, flash and serial-console PowerShell scripts
CLAUDE.md         versioned engineering/agent instructions and decision index
.claude/skills/   subsystem decision records and unresolved hardware choices
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

The `flash-left.ps1` / `flash-right.ps1` scripts are legacy SWD bench flashing
for standalone images. They are not used by the CAN OTA flow.

## STM32 bootloader and CAN update

Build both role-specific bootloaders and applications:

```powershell
.\tools\build-update-images.ps1
```

After applying the documented final CubeMX FDCAN/TIM16 pin migration, use
`-FinalPinout`. A blank STM32 receives its matching custom bootloader once over
USART2 using the factory ROM bootloader. An external Linux service computer
with USB-CAN can then upload application images through SocketCAN:

```bash
python3 tools/ugv_can_update.py --interface can0 --node left \
  --image UGV_STM32_LEFT.bin
```

The updater rejects standalone images linked at the wrong address. Full wiring,
one-time UART provisioning, CAN setup, update, and interrupted-transfer
recovery are in [STM32 firmware update](docs/firmware-update.md).

## Tests

Run all HAL-independent motor math, safety/fault, CAN codec, and DBC-sync
tests with:

```powershell
.\tools\test-host.ps1
```

`tools/build-all.ps1` runs these tests after both STM32 builds unless
`-SkipHostTests` is supplied.

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

The STM32 motor firmware is still at motor-node bring-up stage. The custom
FDCAN bootloader, power-loss-safe flash state machine, application handoff, and
Linux SocketCAN updater are implemented and host-tested. FDCAN is not yet enabled
in the checked-in CubeMX application project. The CD74HC4067 sampling code and
logical channel map are ready, but sensing remains a safe no-op until the
documented GPIO/ADC assignment is added manually and
`UGV_MUX_GPIO_CONFIGURED` is enabled.

See [architecture](docs/architecture.md), [wiring and wire colors](docs/wiring.md),
[CubeMX configuration](docs/cubemx-configuration.md),
[CAN protocol](docs/can-protocol.md), and the node pinout documents under
`docs/`.
