# UGV controller monorepo

Firmware and protocol definitions for a 6×6 UGV with two STM32G431 motor
nodes, one ESP32-S3 control/AUX node, and a Raspberry Pi 5 high-level computer.

## Runtime architecture

| Node | Responsibilities |
| --- | --- |
| STM32 Left | Three left motors, encoders, current sensing, PID and local safety |
| STM32 Right | Three right motors, encoders, current sensing, PID and local safety |
| ESP32 | XR4/CRSF input, arming, skid-steer mixing, two motor UARTs and AUX peripherals |
| Raspberry Pi 5 | Camera, audio, networking, logging and future navigation |

ESP32 talks directly to each STM32 over a private 3.3 V full-duplex UART at
115200 baud. CAN is not used: SN65HVD230 modules, CAN-H/CAN-L, and termination
resistors are removed. See [architecture](docs/architecture.md),
[wiring](docs/wiring.md), and [UART protocol](docs/uart-protocol.md).

## Repository layout

```text
firmware/
  stm32-common/  shared CubeMX/HAL motor-node application
  stm32-left/    left role and direction configuration
  stm32-right/   right role and direction configuration
  esp32/         ESP-IDF control/AUX application
  stm32-bootloader/ legacy CAN bootloader source, not in active runtime
shared/serial/   active framed UART protocol and CRC
shared/can/      legacy CAN protocol retained for history/tests
shared/update/   legacy CAN update protocol retained for history/tests
Tests/           host-side protocol, motor, radio and safety tests
docs/            architecture, wiring, pinouts and build notes
tools/           build, flash and test PowerShell scripts
```

## Build and test

From the repository root:

```powershell
.\tools\build-all.ps1 -SkipEsp32
.\tools\test-host.ps1
```

Or build STM32 targets separately:

```powershell
. .\tools\stm32-env.ps1
cmake --preset stm32-left-debug
cmake --build --preset stm32-left-debug
cmake --preset stm32-right-debug
cmake --build --preset stm32-right-debug
```

Outputs:

- `build/stm32-left-debug/UGV_STM32_LEFT.bin`
- `build/stm32-right-debug/UGV_STM32_RIGHT.bin`

Build ESP32 from an initialized ESP-IDF 6 shell:

```powershell
. C:\esp\v6.0.2\esp-idf\export.ps1
idf.py -C firmware\esp32 build
```

Output: `firmware/esp32/build/ugv_esp32_aux.bin`.

## Flashing

Flash each STM32 with its matching Left/Right standalone image through ST-Link:

```powershell
.\tools\flash-left.ps1
.\tools\flash-right.ps1
```

Flash ESP32 through its onboard USB-UART bridge:

```powershell
idf.py -C firmware\esp32 -p COM3 flash
```

GPIO43/GPIO44 remain available to the ROM downloader. At runtime UART0 is
remapped to the Left link on GPIO39/GPIO40, so application console output is
disabled to prevent log text from entering motor commands. Details and safety
steps are in [firmware-update.md](docs/firmware-update.md).

## Bring-up status

The complete manual path is implemented: XR4 CRSF → ESP32 → independent Left
and Right UART commands → STM32 motor controllers. Commands are role-addressed,
CRC-protected, sent every 20 ms, and locally time out after 300 ms. Both STM32
nodes return RPM/current/safety telemetry at 10 Hz.

ESP32 and both STM32 application images build successfully, and host tests
cover UART framing/resynchronization, CRSF, manual mixing, motor math, and
safety/fault behavior. Hardware validation must start with all wheels raised.
Current-sense scaling is still uncalibrated, so current telemetry validity is
disabled until the physical R_IS/L_IS circuits are verified.
