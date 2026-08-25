# UGV controller architecture

## Runtime topology

```text
 Radio handset + RadioMaster Nomad
          2.4 GHz + 900 MHz Gem-X
                     |
              RadioMaster XR4
                     |
          full-duplex CRSF, 420000 baud
                     |
                     v
            ESP32-S3 control/AUX node <------ Wi-Fi/IP ------ Raspberry Pi 5
            - MANUAL/AUTO arbiter                            - IMX708 camera/audio
            - final motion authority                         - video streaming
            - OLED, QMI8658A, M100-5883                     - future navigation
            - lights and warning buzzer                      - logging/network
                     |
          final Classic CAN commands, 500 kbit/s
                     |
             +-------+-------+
             |               |
             v               v
        STM32 Left       STM32 Right
        3 left motors    3 right motors
        encoders         encoders
        current/temp     current/temp
        local safety     local safety
             |               |
        CD74HC4067       CD74HC4067

 Raspberry Pi 5 ------ Wi-Fi video ------> operator phone/laptop
```

The runtime CAN trunk has exactly three nodes: ESP32, STM32 Left, and STM32
Right. Each uses its own SN65HVD230 transceiver. Raspberry Pi has no CAN
transceiver and no CAN wiring. The two STM32 nodes close their own speed-control
and safety loops. The ESP32 chooses the active command source but never
performs motor PID control.

Manual control is independent of Raspberry Pi and Wi-Fi. If either fails, the
ELRS/XR4 to ESP32 to CAN path remains operational; only video, Pi logging, and
future Pi autonomy are lost.

## STM32 boot and update path

Each STM32 has a role-specific custom bootloader at `0x08000000` and an OTA
application at `0x08006000`. During maintenance, an external Linux service
computer and USB-CAN adapter temporarily connect to the 500 kbit/s CAN trunk.
The running motor application accepts `ENTER` only from a safe
disabled/fault/estop state, disables all motor outputs, and resets into the
bootloader.

The bootloader invalidates application metadata before erasing any application
page. It writes sequenced six-byte CAN chunks, verifies the complete image
CRC-32 and vector table, then commits the metadata magic last. A power failure
during transfer therefore leaves the STM32 in CAN recovery instead of booting
a partial program. There is one application slot, so this is safe recovery but
not A/B rollback.

A blank MCU needs the custom bootloader installed once through the factory ROM
USART2 interface on PA2/PA3 with BOOT0/PB8. The STM32G431 ROM bootloader does
not expose FDCAN. After that first provisioning, ST-Link and UART are not
needed for normal application updates. See
[`firmware-update.md`](firmware-update.md).

## Control authority

Only the ESP32 may publish the final `VehicleMotion` (`0x100`) and
`SystemEnable` (`0x110`) frames consumed by the motor nodes. A future Raspberry
Pi autonomy service sends requests to ESP32 over an authenticated Wi-Fi/IP
protocol, never directly to CAN. This keeps Raspberry Pi outside the critical
motor bus and prevents competing final command producers.

| Selected mode | Accepted source | Failure behavior |
| --- | --- | --- |
| `DISABLED` | None | Final targets are zero and motor enable is false |
| `MANUAL` | Valid XR4 CRSF link | RC loss/failsafe stops and disables the vehicle |
| `AUTO` | Fresh Raspberry Pi Wi-Fi request | Stale network request stops and disables the vehicle |

The operator selects the mode explicitly. Failure of the selected source must
not cause an automatic change to another source; for example, loss of the Pi
in AUTO never activates a non-neutral RC stick unexpectedly.

## Safety chain

```text
XR4 frame age/CRSF failsafe       100 ms
              |
Pi Wi-Fi AUTO request age         300 ms
              |
ESP32 final command period        10-20 ms
              |
STM32 final command timeout       300 ms
              |
local current/temperature/encoder checks
              |
motor-driver enable outputs
```

The timeouts are declared in `shared/can/ugv_can_protocol.h`. Any reset starts
disabled and requires explicit re-arm. A physical emergency-stop must remove
motor drive capability independently of ESP32, Raspberry Pi, CAN, and normal
software execution.

## Responsibility boundaries

- STM32 nodes own motor PWM/enables, quadrature encoders, local current and
  temperature sampling, final-command timeout, target reset, and driver
  disable.
- ESP32 owns XR4/CRSF input, MANUAL/AUTO arbitration, final vehicle commands,
  the local display/control panel, QMI8658A IMU, M100-5883 GPS/compass,
  future light sensor, lighting outputs, warning buzzer, and the future Wi-Fi
  command/telemetry gateway.
- Raspberry Pi owns the IMX708 camera, full speaker/audio path, navigation,
  video streaming, future Wi-Fi autonomy requests, telemetry storage, and
  logging. It never publishes or receives CAN frames directly.

## Source ownership

`firmware/stm32-common` is the only copy of generated STM32 code, HAL drivers,
motor control, safety, and platform adapters. `firmware/stm32-left` and
`firmware/stm32-right` contain target identity and calibration only.

`firmware/esp32` is an independent ESP-IDF project. It does not include or
modify the older `blinkESP32` repository.

`shared/can` is platform-neutral and is consumed by STM32, ESP32, service
tools, and host tests. Raspberry Pi uses a separate Wi-Fi/IP contract with
ESP32. C structures are never copied directly to CAN data; the codec defines
byte order and payload length explicitly.

## RF and GPS placement

Mount the XR4 and its two antennas away from the GPS receiver/antenna, motor
wiring, DC/DC converters, and CAN transceivers. Validate GPS satellite count
and fix quality with Gem-X transmitting before fixing the final enclosure
layout. This is especially important because XR4 Xrossband installations have
reported GPS interference when RF and GNSS hardware are placed close together.

## Current implementation boundary

The CAN update protocol, STM32 bootloader, flash validation/recovery logic,
application-to-bootloader handoff, and Linux SocketCAN service uploader are
implemented and host-tested, but not yet validated on assembled hardware. The
checked-in CubeMX project still has the legacy pinout; its manual FDCAN/TIM16/
MUX migration is documented separately.

The application-side FDCAN code currently owns only the update-entry filter.
The normal STM32 motion-command and telemetry dispatcher, ESP32 CRSF parsing,
ESP32 arbitration/TWAI transport, Pi camera streaming, and the future ESP32-to-
Pi Wi-Fi protocol remain separate milestones. Until those transports are
finished and hardware-tested, USART2 remains the motor-node bench command
interface.
