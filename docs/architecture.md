# UGV controller architecture

## Runtime topology

```text
Radio handset + Nomad ))) RF ))) XR4
                              |
                      CRSF UART 420000
                              |
                          ESP32-S3 <--- Wi-Fi/IP ---> Raspberry Pi 5
                         /         \
       UART0 115200     /           \     UART2 115200
       GPIO39/40       v             v    GPIO41/42
                  STM32 Left     STM32 Right
                  PA3 RX/PA2 TX  PA3 RX/PA2 TX
                   3 motors       3 motors
```

CAN is not used in the runtime architecture. The three SN65HVD230 modules,
CAN-H/CAN-L wiring, and 120-ohm termination are removed and unpowered. ESP32
has one private full-duplex 3.3 V UART link to each motor node. Both links run
at 115200 baud, 8 data bits, no parity, one stop bit.

The ESP32 chooses the active command source and sends a separate three-wheel
command every 20 ms to each STM32. Each STM32 closes its own PID and safety
loops, validates the target role and CRC, and disables its drivers when no
valid command arrives for 300 ms. The STM32 nodes return RPM, current fields,
safety state, and validity/fault masks at 10 Hz.

Manual control remains independent of Raspberry Pi and Wi-Fi. Raspberry Pi
owns video, logging, navigation, and future autonomy requests; it is not in
the motor safety path.

## Control authority and safety

Only ESP32 produces final left/right motor commands. A frame contains a role
(`LEFT` or `RIGHT`), three independent signed RPM targets, a three-bit wheel
enable mask, ARM/ESTOP flags, a sequence number, and CRC-16-CCITT. A node
rejects a command for the opposite role, so swapping the two UART harnesses
cannot drive the wrong side.

```text
XR4 frame timeout              100 ms
ESP32 command period            20 ms
STM32 valid-command timeout    300 ms
local encoder/current checks
physical emergency disconnect
```

Any reset begins disabled. ARM requires the configured low-to-high switch
edge with neutral controls. Loss of CRSF makes ESP32 send zero/disarm, and
loss or corruption of UART commands independently times out each STM32.
A physical emergency stop must still remove motor-drive capability without
depending on any MCU or communication link.

## Power boundaries

One MP1584EN adjusted to 3.30 V supplies the ESP32, both STM32 boards, six
encoders, IBT-2 logic, and other 3.3 V peripherals through separate star
branches. Removing three CAN transceivers slightly reduces this load. A second
MP1584EN adjusted to 5.00 V supplies XR4 and M100-5883. Motor power, Raspberry
Pi 5 V, and lights use their own fused branches.

Every UART endpoint must share logic ground. Motor-current returns must not
flow through those signal-ground wires.

## Firmware and ownership

- `shared/serial` defines the platform-neutral framed UART protocol.
- `firmware/esp32` owns CRSF parsing, arming, drive mixing, both motor UARTs,
  and telemetry forwarding.
- `firmware/stm32-common` owns USART2 command reception, local motor control,
  encoders, current sampling, faults, watchdog, and telemetry.
- `firmware/stm32-left` and `firmware/stm32-right` contain role and direction
  configuration only.

The former CAN bootloader/update implementation remains only as legacy source
history and is not part of the current application builds. Current STM32
application updates use ST-Link; see [firmware-update.md](firmware-update.md).

## Current implementation boundary

The dual-UART runtime path is implemented and build-tested for ESP32, STM32
Left, and STM32 Right. The host suite tests framing, byte order, CRC rejection,
parser resynchronization, manual mixing, and STM32 safety logic. Hardware
validation must still begin with all wheels raised and motor power readily
disconnectable. Current-sense scaling remains uncalibrated.
