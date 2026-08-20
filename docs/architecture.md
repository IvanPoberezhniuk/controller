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
            ESP32-S3 control/AUX node <-------- CAN -------- Raspberry Pi 5
            - MANUAL/AUTO arbiter                            - camera/audio
            - final motion authority                         - navigation
            - OLED, IMU, GPS, light sensor                   - autonomy request
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
```

Every CAN node has its own external transceiver. The two STM32 nodes close
their own speed-control and safety loops. The ESP32 chooses the active command
source but never performs motor PID control.

## Control authority

Only the ESP32 may publish the final `VehicleMotion` (`0x100`) and
`SystemEnable` (`0x110`) frames consumed by the motor nodes. Raspberry Pi uses
the separate `AutoMotionRequest` (`0x101`) and `AutoEnableRequest` (`0x111`)
frames. This keeps normal software from producing two competing command
streams with the same CAN identifiers.

| Selected mode | Accepted source | Failure behavior |
| --- | --- | --- |
| `DISABLED` | None | Final targets are zero and motor enable is false |
| `MANUAL` | Valid XR4 CRSF link | RC loss/failsafe stops and disables the vehicle |
| `AUTO` | Fresh Raspberry Pi request | Stale Pi request stops and disables the vehicle |

The operator selects the mode explicitly. Failure of the selected source must
not cause an automatic change to another source; for example, loss of the Pi
in AUTO never activates a non-neutral RC stick unexpectedly.

## Safety chain

```text
XR4 frame age/CRSF failsafe       100 ms
              |
Pi AUTO request age               300 ms
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
  the local display/control panel, IMU, light sensor, GPS, lighting outputs,
  and warning buzzer.
- Raspberry Pi owns the IMX708 camera, full speaker/audio path, navigation,
  autonomy requests, remote connectivity, telemetry storage, and logging.

## Source ownership

`firmware/stm32-common` is the only copy of generated STM32 code, HAL drivers,
motor control, safety, and platform adapters. `firmware/stm32-left` and
`firmware/stm32-right` contain target identity and calibration only.

`firmware/esp32` is an independent ESP-IDF project. It does not include or
modify the older `blinkESP32` repository.

`shared/can` is platform-neutral and is consumed by STM32, ESP32, Raspberry Pi
software, and host tests. C structures are never copied directly to CAN data;
the codec defines byte order and payload length explicitly.

## RF and GPS placement

Mount the XR4 and its two antennas away from the GPS receiver/antenna, motor
wiring, DC/DC converters, and CAN transceivers. Validate GPS satellite count
and fix quality with Gem-X transmitting before fixing the final enclosure
layout. This is especially important because XR4 Xrossband installations have
reported GPS interference when RF and GNSS hardware are placed close together.

## Current implementation boundary

The repository now defines the final pin allocation and CAN wire contract for
this topology. CRSF parsing, ESP32 command arbitration, TWAI transport, and the
STM32 FDCAN transport are separate bring-up milestones and are not yet claimed
as hardware-tested. Until those transports are implemented, the STM32 debug
UART remains the bench command interface.
