# Firmware update

## Persistent UART bootloader

Each STM32 can be provisioned once over ST-Link with an immutable 24 KiB
bootloader, an application linked at `0x08006000`, and CRC-protected metadata:

```powershell
.\tools\install-stm32-uart-bootloader.ps1 -Node Left
.\tools\install-stm32-uart-bootloader.ps1 -Node Right
```

The bootloader uses the existing 115200-baud USART2 link (`PA2` TX, `PA3` RX)
to the ESP32. It always forces all six PWM pins and all three common BTS enable
nets low before checking or updating flash. A running application accepts the
bootloader-entry command only while its outputs are already safe (`DISABLED`,
`FAULT`, or `EMERGENCY_STOP`) and only with the update magic value, then
disables every motor output before reset.

The application occupies `0x08006000..0x0801F7FF`; the final 2 KiB flash page
at `0x0801F800` stores image size, role, generation, and CRC-32. Metadata is
invalidated before an erase and committed only after the complete image and
vector table verify. An interrupted update therefore stays in recovery mode
instead of executing a partial image.

Before jumping to a valid application, the bootloader disables and clears all
pending NVIC sources, relocates `VTOR`, installs the application's main stack,
and restores global interrupts (`PRIMASK=0`). Restoring `PRIMASK` is required:
without it the application can transmit/poll peripherals but USART2 RX and all
other interrupts remain globally blocked. The application also restores global
interrupts after initializing its UART RX ring as a defensive second layer.

The ESP32 firmware includes the STM32 UART image transport. Hold the ESP32
`BOOT` button for approximately two seconds while the normal application is
running to enter maintenance mode. Do not hold `BOOT` during ESP32 reset or
power-up, because that selects the ESP32 ROM downloader instead. The PC sends
the matching Left or Right OTA image through the ESP32 USB-UART connection;
the ESP32 validates the image header, size, role, vector table, and CRC before
forwarding it to the selected STM32.

With the ESP32 USB-UART bridge on `COM3`, start the host sender and then hold
`BOOT` for approximately two seconds when prompted:

```powershell
.\tools\update-stm32-via-esp.ps1 -Node Left -Port COM3
.\tools\update-stm32-via-esp.ps1 -Node Right -Port COM3
```

## Direct SWD recovery

For recovery or development **without** the persistent bootloader layout,
flash each standalone STM32 application over SWD with the ST-Link V2 clone:

```powershell
.\tools\flash-left.ps1
.\tools\flash-right.ps1
```

Use the matching image for each physical side:

| Board | Image |
| --- | --- |
| Left | `build/stm32-left-debug/UGV_STM32_LEFT.bin` |
| Right | `build/stm32-right-debug/UGV_STM32_RIGHT.bin` |

These standalone commands place the application at `0x08000000` and replace
the persistent bootloader layout. For a board that should retain UART updates,
use `install-stm32-uart-bootloader.ps1 -Node Left|Right` instead; it writes the
bootloader, relocated application, and matching CRC metadata together.

ST-Link wiring is `SWDIO→PA13`, `SWCLK→PA14`, `GND→GND`, and optionally
`NRST→NRST`. If the vehicle's regulated 3.3 V rail powers the STM32, do not
also connect the ST-Link 3.3 V output. Remove motor fuses or otherwise isolate
12 V motor power while flashing.

ESP32 is flashed through its onboard USB-UART bridge:

```powershell
. C:\esp\v6.0.2\esp-idf\export.ps1
idf.py -C firmware\esp32 -p COM3 flash
```

The ESP application remaps UART0 to GPIO39/GPIO40 only after boot. The ROM
downloader still uses GPIO43/GPIO44, so normal USB flashing remains available.

## Legacy source

`tools/ugv_can_update.py` and `firmware/stm32-bootloader/Platform/Src/ugv_boot_can.c`
are retained as legacy CAN references only. The active bootloader transport is
UART. The shared update state machine, CRC, flash writer, and metadata format
remain transport-independent.
