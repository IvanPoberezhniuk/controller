# STM32 firmware update without ST-Link

The two motor nodes use a small, role-specific CAN bootloader. ST-Link is not
required in normal operation. A blank STM32 must first receive the bootloader
once through the factory ROM USART2 bootloader; all later application updates
come from a temporary Linux/SocketCAN service host over Classic CAN at
500 kbit/s. The vehicle Raspberry Pi remains Wi-Fi-only and is not used for
this maintenance connection.

The STM32G431 system-memory bootloader does not support FDCAN, so a completely
blank MCU cannot be provisioned directly through CAN. The custom bootloader in
this repository adds that capability after the one-time UART step.

## What is built

Run from the repository root on the development PC:

```powershell
.\tools\build-update-images.ps1
```

After the final CubeMX pin migration described in
[`cubemx-configuration.md`](cubemx-configuration.md), build with the real CAN
and TIM16 pinout enabled:

```powershell
.\tools\build-update-images.ps1 -FinalPinout
```

Do not use `-FinalPinout` before CubeMX has generated `hfdcan1`, `htim16`, and
the final GPIO labels. The outputs are:

| Node | One-time image at `0x08000000` | CAN application image |
| --- | --- | --- |
| Left | `build/stm32-left-bootloader-release/UGV_BOOTLOADER_LEFT.bin` | `build/stm32-left-ota-release/UGV_STM32_LEFT.bin` |
| Right | `build/stm32-right-bootloader-release/UGV_BOOTLOADER_RIGHT.bin` | `build/stm32-right-ota-release/UGV_STM32_RIGHT.bin` |

Never interchange Left and Right bootloaders or applications. The node identity
selects different CAN data/status identifiers and is also stored in the image
metadata. A normal `stm32-*-release` application is linked at `0x08000000` and
is not a valid OTA image; the SocketCAN updater rejects it.

## One-time provisioning through USB-UART

Use a 3.3 V USB-UART adapter. Do not use a 5 V logic-level adapter. Disconnect
motor power and make sure every BTS7960 enable has a hardware pull-down before
starting.

| USB-UART / control | STM32G431 | Color | Note |
| --- | --- | --- | --- |
| Adapter TX | `PA3 / USART2_RX` | White | Signals cross |
| Adapter RX | `PA2 / USART2_TX` | Orange | Signals cross |
| Adapter GND | `GND` | Black | Common reference |
| `3V3` through removable jumper | `PB8 / BOOT0` | Violet | High only while entering ROM bootloader |
| Reset button/test lead | `NRST` | Gray | Pulse low after BOOT0 is high |

Fit a 10 kohm pull-down from PB8/BOOT0 to GND. PB8 is also the final center
motor `LPWM` output (`TIM16_CH1`), so the BOOT0 jumper must be open before motor
power is restored.

Provision each board separately:

1. Disconnect the CAN transceiver or leave the CAN bus unpowered; disconnect
   motor power.
2. Connect TX, RX, and GND as shown above.
3. Pull PB8/BOOT0 high, then pulse NRST low or power-cycle the STM32.
4. In STM32CubeProgrammer select **UART**, 115200 8E1, and connect.
5. Program the matching `UGV_BOOTLOADER_LEFT.bin` or
   `UGV_BOOTLOADER_RIGHT.bin` at address `0x08000000`, then verify it.
6. Disconnect CubeProgrammer, remove the BOOT0 jumper, and reset the board.
7. The custom bootloader now keeps the motor outputs low and waits on CAN,
   because no valid application metadata exists yet.

USART2 is needed only for this first installation or deep recovery. SWD pads
may remain on the PCB as optional debug/test points, but they are not part of
the update path.

## Temporary SocketCAN service connection

Each STM32 and ESP32 uses its permanent SN65HVD230. To service firmware,
connect an external Linux computer through a USB-CAN adapter that exposes a
SocketCAN interface. This adapter is temporary and does not make the onboard
Raspberry Pi a CAN node. Once Linux exposes `can0`, configure it:

```bash
sudo ip link set can0 down 2>/dev/null || true
sudo ip link set can0 type can bitrate 500000 restart-ms 100
sudo ip link set can0 up
ip -details -statistics link show can0
```

Copy `tools/ugv_can_update.py` and the required OTA `.bin` to that service
computer, or run them from a checkout of this repository. The updater uses
Python's standard library and Linux SocketCAN; it does not require
`python-can`.

For the first application immediately after UART provisioning, the node is
already in the custom bootloader:

```bash
python3 tools/ugv_can_update.py \
  --interface can0 --node left \
  --image build/stm32-left-ota-release/UGV_STM32_LEFT.bin \
  --no-enter
```

Repeat with `--node right` and the Right image. For all later updates, while a
valid application is running, omit `--no-enter`:

```bash
python3 tools/ugv_can_update.py \
  --interface can0 --node left \
  --image UGV_STM32_LEFT.bin
```

The running application accepts the bootloader request only while its control
state is `DISABLED`, `FAULT`, or `ESTOP`. It first disables all drivers and
zeros PWM, writes a reset request into reserved SRAM, then resets. Do not try to
update a moving or enabled vehicle.

## Update transaction and recovery

```text
Service host                running app / bootloader
     |--- ENTER (0x600) ---> disable outputs, reset
     |--- QUERY (0x600) ---> status READY/IDLE (0x680 or 0x681)
     |--- BEGIN(size) -----> invalidate old metadata, erase app pages
     |=== DATA, 6 B/frame => ACK every 32 frames
     |--- FINISH(CRC-32) -> verify vectors + complete flash CRC
     |<-- VERIFIED -------- commit metadata last
     |--- ACTIVATE -------> reset and start application
```

Firmware data uses `0x610` for Left and `0x611` for Right. Status uses `0x680`
and `0x681`. Frames contain a sequence number; duplicates are acknowledged and
gaps return the next expected sequence. The service host queries progress and
retries a window after a lost ACK.

If power or CAN is lost after `BEGIN`, valid metadata has already been erased.
The bootloader will not jump into a partial image and will wait on CAN after the
next reset. Restore power/bus and rerun the matching update with `--no-enter`.
The current design does not keep a second application slot, so the previous
application is not available for rollback.

## Flash map

| Region | Address | Size | Purpose |
| --- | ---: | ---: | --- |
| Custom bootloader | `0x08000000` | 24 KiB | Safe boot, FDCAN receiver, flash writer |
| Application | `0x08006000` | up to 102 KiB | Left or Right motor firmware |
| Metadata page | `0x0801F800` | 2 KiB | Node, size, CRC-32, generation, header CRC |
| Boot request | `0x20007FF8` | 8 B SRAM | Magic plus complement across software reset |

The metadata magic is programmed last, only after the exact image size, vector
table, and CRC-32 have been verified. On every cold boot the bootloader checks
the metadata, stack/reset vectors, node identity, and complete application
CRC before jumping.

## Bench acceptance checklist

- Motor supply disconnected; all six R_EN/L_EN lines measure low during reset
  and while the bootloader waits.
- Power-off CAN-H to CAN-L resistance is approximately 60 ohm with exactly two
  120 ohm end terminators.
- `ip -details link show can0` reports 500000 bit/s and no rapidly increasing
  bus errors.
- Left update receives only `0x680`; Right update receives only `0x681`.
- Interrupting an update, resetting, and rerunning with `--no-enter` recovers.
- A wrong-role or normal-address `.bin` is rejected and is never activated.
- After activation, the application remains disabled until a fresh explicit
  enable command arrives.
