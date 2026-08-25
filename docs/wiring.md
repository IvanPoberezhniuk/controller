# UGV wiring diagram

This file is the harness reference for the current architecture. Label both
ends of every wire with the signal name; color alone is not an identifier.

`FINAL` means the pin is already represented in source/configuration.
`PLANNED CubeMX` means the allocation is fixed in the bootloader and hand-written
code, but the generated STM32 `.ioc` still needs the documented manual edit.
Do not power the motor harness until that regeneration is complete.

## System interconnect

```text
 Radio handset + Nomad )))) 2.4/900 MHz (((( XR4
                                               |
                                  CRSF TX/RX + 5 V + GND
                                               |
                                               v
                                         ESP32-S3
                                         SN65HVD230
                                               |
 CAN end A [120R] -- STM32 Left -- ESP32 -- STM32 Right -- [120R] CAN end B
                         SN65HVD230 at every node

 IMX708 camera --> Raspberry Pi 5 )))) Wi-Fi video )))) operator device
                         )))) optional Wi-Fi/IP )))) ESP32
                         no connection to CAN-H/CAN-L
```

The diagram shows logical order only. Put the terminators on the two physical
ends of the installed trunk, regardless of which nodes happen to be there.
Keep node stubs short and do not wire CAN as a passive star.

## Harness color convention

| Signal family | Color | Rule |
| --- | --- | --- |
| Positive supply | Red | Add a voltage label: `VBAT`, `5V`, or `3V3` |
| Ground/reference | Black | `GND`; never use black for a driven signal |
| CAN-H | Yellow | Twist together with CAN-L |
| CAN-L | Green | Twist together with CAN-H |
| Device/transceiver to controller | White | UART TX or CAN RXD entering an MCU |
| Controller to device/transceiver | Orange | UART TX or CAN TXD leaving an MCU |
| I2C SDA | Blue | Use the same convention on both I2C buses |
| I2C SCL | Yellow | Separate harness from CAN to avoid confusion |
| Interrupt/button | White | Add a printed signal label |
| Unassigned/reserve | No wire | Do not pre-wire TBD pins |

For motor-driver harnesses, use the dedicated convention in the STM32 section.

## RadioMaster XR4 to ESP32

Use the XR4 primary CRSF pads, not the secondary UART.

| From | To | Color | Status / note |
| --- | --- | --- | --- |
| XR4 `5V` | Regulated 5 V receiver rail | Red | FINAL; do not connect to ESP32 `3V3` |
| XR4 `GND` | ESP32/power ground | Black | FINAL; common signal reference |
| XR4 `TX` | ESP32 `GPIO21` (`CRSF_RX`) | White | FINAL; UART signals cross |
| XR4 `RX` | ESP32 `GPIO38` (`CRSF_TX`) | Orange | FINAL; carries telemetry to the radio |

CRSF is configured as non-inverted, full-duplex UART at 420000 baud. RadioMaster
does not state a guaranteed UART logic voltage in the published XR4 summary.
Measure XR4 TX before direct connection: if its high level exceeds the ESP32
input limit, add a proper high-speed level translator. Never assume that a
device powered from 5 V necessarily has 3.3 V UART pads.

Place local decoupling at the receiver power input. Route XR4 power and UART
away from motor leads. Mount the XR4 and both RF antennas as far as practical
from the GPS antenna, then test GPS fix quality while Gem-X is transmitting.

## ESP32-S3 peripheral wiring

Target board: Sixspan ESP32-S3-N16R8. GPIO39-GPIO42 remain free after all
assignments below.

### SN65HVD230 CAN transceiver

| From | To | Color | Status / note |
| --- | --- | --- | --- |
| ESP32 `GPIO17` (`TWAI_TX`) | SN65HVD230 `D/TXD` | Orange | FINAL; add 10 kohm pull-up to 3V3 |
| SN65HVD230 `R/RXD` | ESP32 `GPIO18` (`TWAI_RX`) | White | FINAL; logic-side signal |
| ESP32 `3V3` | SN65HVD230 `VCC` | Red, label `3V3` | Never power this part from 5 V |
| ESP32 `GND` | Transceiver `GND` | Black | Common signal reference |
| Transceiver `CANH` | CAN-H trunk | Yellow | Twisted pair |
| Transceiver `CANL` | CAN-L trunk | Green | Twisted pair |
| SN65HVD230 `RS` | `GND` | Black | High-speed mode; often handled on modules |
| SN65HVD230 `Vref` | Not connected | No wire | Leave floating when unused |

Place 100 nF directly across transceiver VCC/GND and a CAN TVS such as
SM24CANB close to the bus connector. ESP32 GPIOs must never connect directly
to CAN-H/CAN-L.

### SH1106 OLED

| OLED pin | ESP32 | Color | Status / note |
| --- | --- | --- | --- |
| `VCC` | `3V3` | Red, label `3V3` | FINAL; use 3.3 V |
| `GND` | `GND` | Black | FINAL |
| `SDA` | `GPIO8` | Blue | FINAL; I2C address `0x3C` |
| `SCL` | `GPIO9` | Yellow | FINAL; planned 400 kHz |

### Rotary encoder UI

| Encoder pin | ESP32 | Color | Status / note |
| --- | --- | --- | --- |
| `A/CLK` | `GPIO4` | Blue | FINAL |
| `B/DT` | `GPIO5` | Green | FINAL |
| `SW` | `GPIO6` | White | FINAL; input with pull-up |
| `VCC` | `3V3` | Red, label `3V3` | If the module requires power |
| `GND` | `GND` | Black | FINAL |

### QMI8658A IMU, M100-5883 compass, and ambient-light I2C bus

The QMI8658A IMU and the compass integrated in the HGLRC M100-5883 share the
sensor I2C bus with the still-unselected ambient-light sensor. Start this bus
at 100 kHz because the compass cable runs away from the ESP32 enclosure.

| Device pin | ESP32 | Color | Status / note |
| --- | --- | --- | --- |
| QMI8658A `VCC` | `3V3` | Red, label `3V3` | FINAL; the IC itself must not receive 5 V |
| QMI8658A `GND` | `GND` | Black | FINAL |
| QMI8658A `SDA` | `GPIO1` | Blue | FINAL; expected address `0x6A` or `0x6B` from SA0 |
| QMI8658A `SCL` | `GPIO2` | Yellow | FINAL |
| QMI8658A `INT1/DRDY`, if exposed | `GPIO7` | White | FINAL but optional; otherwise poll over I2C |
| M100-5883 `SDA` | `GPIO1` | Blue | FINAL; integrated compass, expected `0x0D`, verify by scan |
| M100-5883 `SCL` | `GPIO2` | Yellow | FINAL |
| Ambient-light `SDA/SCL` | `GPIO1/GPIO2` | Blue / Yellow | PLANNED; select a non-conflicting address |

QMI8658A supports 7-bit I2C address `0x6A` when SA0 is high/unconnected and
`0x6B` when SA0 is low. Its `WHO_AM_I` register at `0x00` should return `0x05`.
The exact 5883-compatible compass fitted to individual HGLRC module revisions
must be confirmed by an I2C scan rather than assumed from the product name.

Install only one effective SDA pull-up and one SCL pull-up set for the complete
bus. Before connection, power each module separately and verify that idle SDA
and SCL do not exceed 3.3 V. Parallel pull-ups already fitted to the QMI,
M100-5883, and future light-sensor boards can become too strong.

### HGLRC M100-5883 GPS UART and compass

The confirmed module is HGLRC M100-5883: M10-class multi-constellation GPS
with integrated compass, default UART 115200 baud and 10 Hz navigation output.

| M100-5883 pin | ESP32 | Color | Status / note |
| --- | --- | --- | --- |
| `TX` | `GPIO16` (`GPS_RX`) | White | FINAL; UART signals cross |
| `RX` | `GPIO15` (`GPS_TX`) | Orange | FINAL |
| `SDA` | `GPIO1` | Blue | FINAL; compass I2C, not GPS UART data |
| `SCL` | `GPIO2` | Yellow | FINAL; compass I2C |
| `GND` | `GND` | Black | FINAL |
| `5V` | Regulated `5V` sensor rail | Red, label `5V` | FINAL; HGLRC specifies 3.3-5 V input |

The manufacturer does not separately specify UART/I2C pad-high voltage. Power
the module from 5 V on the bench and measure idle TX, SDA, and SCL before direct
ESP32 connection; all must be at most 3.3 V. Capture the initial UART stream to
identify its actual NMEA/vendor protocol before implementing the parser.

Mount the ceramic GPS antenna facing the sky. Keep the module away from XR4
antennas, motors, steel brackets, speakers, battery leads, and DC/DC converters.
After final assembly perform compass offset/scale calibration in the installed
position; calibration done on the bench is invalid after nearby metal or
high-current wiring moves.

### Lights and buzzer

These GPIOs drive MOSFET/driver inputs only. Never power a lamp or high-current
buzzer directly from ESP32.

| Function | ESP32 output | Color | Load-side requirement |
| --- | --- | --- | --- |
| Front light PWM | `GPIO10` | Blue | MOSFET/driver, fuse |
| Rear light PWM | `GPIO11` | Brown | MOSFET/driver, fuse |
| Left indicator | `GPIO12` | Green | MOSFET/driver |
| Right indicator | `GPIO13` | Yellow | MOSFET/driver |
| Warning buzzer | `GPIO14` | Violet | Transistor/driver; flyback diode if inductive |
| Driver reference | `GND` | Black | Common logic ground |

### Reserved and free ESP32 pins

| GPIO | Use |
| --- | --- |
| `19/20` | Native USB D-/D+; reserved |
| `43/44` | UART0 programming/logging; reserved |
| `47` | Optional service LED |
| `48` | Onboard addressable RGB LED |
| `39/40/41/42` | Free safe GPIO reserve |
| `0/3/45/46` | Boot-strapping pins; do not allocate in this harness |
| `26-37` | Excluded because of flash/PSRAM/board restrictions |

## STM32 Left and STM32 Right

Both STM32G431 nodes use the same MCU pin layout. `motor0` means front,
`motor1` center, and `motor2` rear. The Left board connects those signals to
the three left wheels; the Right board connects them to the three right wheels.

### Encoder and final motor-driver pin plan

| Motor | Function | STM32 pin | Motor harness color | Status |
| --- | --- | --- | --- | --- |
| Front / motor0 | Encoder A | `PA0` | Brown | FINAL |
| Front / motor0 | Encoder B | `PA1` | Pink | FINAL |
| Front / motor0 | RPWM | `PA8` | Orange | FINAL |
| Front / motor0 | LPWM | `PA9` | Yellow | FINAL |
| Front / motor0 | R_EN | `PB0` | Green | FINAL |
| Front / motor0 | L_EN | `PB1` | Blue | FINAL |
| Center / motor1 | Encoder A | `PB4` | Brown | FINAL |
| Center / motor1 | Encoder B | `PB5` | Pink | FINAL |
| Center / motor1 | RPWM | `PA10` | Orange | FINAL |
| Center / motor1 | LPWM | `PB8` (`TIM16_CH1`) | Yellow | PLANNED CubeMX |
| Center / motor1 | R_EN | `PA4` | Green | PLANNED CubeMX |
| Center / motor1 | L_EN | `PA5` | Blue | PLANNED CubeMX |
| Rear / motor2 | Encoder A | `PB6` | Brown | FINAL |
| Rear / motor2 | Encoder B | `PB7` | Pink | FINAL |
| Rear / motor2 | RPWM | `PB14` | Orange | FINAL |
| Rear / motor2 | LPWM | `PB15` | Yellow | FINAL |
| Rear / motor2 | R_EN | `PB10` | Green | FINAL |
| Rear / motor2 | L_EN | `PB11` | Blue | FINAL |

For every encoder also run its rated supply as red with a voltage label and
ground as black. Confirm whether the encoder outputs are safe at STM32 3.3 V
before connection. The same signal colors repeat only inside each separately
labeled front/center/rear connector.

### Local CD74HC4067 on each STM32

| MUX channel | Source | Color at driver/sensor connector |
| ---: | --- | --- |
| `C0` | Front `R_IS` | White |
| `C1` | Front `L_IS` | Gray |
| `C2` | Center `R_IS` | White |
| `C3` | Center `L_IS` | Gray |
| `C4` | Rear `R_IS` | White |
| `C5` | Rear `L_IS` | Gray |
| `C6-C15` | Reserved; leave unconnected | No wire |

MUX `VCC` goes to STM32 `3V3` with a red wire labelled `3V3`; MUX `GND` and
`EN` go to ground with black wires so the chip is always enabled. Place a
100 nF decoupling capacitor directly between MUX VCC and GND.

Only `C0-C5` are populated in the current build. Every R_IS/L_IS signal must
be conditioned so its complete voltage range stays within `0-3.3 V`. Add
dividers, buffers, and input protection as required by the final motor driver;
the multiplexer itself does not make a 5 V signal safe for the STM32 ADC.

### CD74HC4067 control and ADC wiring

| CD74HC4067 | STM32 pin | Color | Status / note |
| --- | --- | --- | --- |
| `SIG` | `PA6 / ADC2_IN3` | White | PLANNED CubeMX; single-ended ADC input |
| `S0` | `PA7` | Blue | PLANNED CubeMX; GPIO output, initial low |
| `S1` | `PB2` | Green | PLANNED CubeMX; GPIO output, initial low |
| `S2` | `PB12` | Yellow | PLANNED CubeMX; GPIO output, initial low |
| `S3` | `PB13` | Violet | PLANNED CubeMX; GPIO output, initial low |
| `EN` | `GND` | Black | Always enabled |
| `VCC` | `3V3` | Red, label `3V3` | 100 nF local decoupling |
| `GND` | `GND` | Black | Same analog reference as STM32 |

The generated `main.h` currently contains legacy direct-ADC labels. They are
not the final harness. Enable `UGV_MUX_GPIO_CONFIGURED` only after CubeMX has
generated `MUX_SIG` and `MUX_S0` through `MUX_S3` exactly as above.

### STM32 SN65HVD230 CAN transceiver

| From | To | Color | Status / note |
| --- | --- | --- | --- |
| STM32 `PA12 / FDCAN1_TX` | SN65HVD230 `D/TXD` | Orange | PLANNED CubeMX; 10 kohm pull-up to 3V3 |
| SN65HVD230 `R/RXD` | STM32 `PA11 / FDCAN1_RX` | White | PLANNED CubeMX |
| STM32 `3V3` | SN65HVD230 `VCC` | Red, label `3V3` | Never connect to 5 V |
| STM32 `GND` | Transceiver `GND` | Black | Common signal reference |
| Transceiver `CANH` | CAN-H trunk | Yellow | Twisted with CAN-L |
| Transceiver `CANL` | CAN-L trunk | Green | Twisted with CAN-H |
| SN65HVD230 `RS` | `GND` | Black | High-speed mode; often handled on modules |
| SN65HVD230 `Vref` | Not connected | No wire | Leave floating when unused |

PA11/PA12 connect only to the transceiver logic pins, never directly to
CAN-H/CAN-L. Place 100 nF at VCC/GND and SM24CANB at the bus connector. The
bootloader already configures these pins; the application starts using them
after the manual CubeMX migration.

### STM32 service connections

| Function | STM32 pin | Color | Status |
| --- | --- | --- | --- |
| USART2 TX / adapter RX | `PA2` | Orange | Console and factory ROM provisioning |
| USART2 RX / adapter TX | `PA3` | White | Console and factory ROM provisioning |
| BOOT0 jumper to `3V3` | `PB8` | Violet | Provisioning only; normally open, 10 kohm pull-down |
| SWDIO | `PA13` | Blue | Optional debug/recovery test pad |
| SWCLK | `PA14` | Yellow | Optional debug/recovery test pad |
| NRST | `NRST` | Gray | Reset/programming test point |
| Ground | `GND` | Black | Programming/debug reference |

PB8 is both BOOT0 and the final center `LPWM`. Never fit the BOOT0 jumper while
motor power is connected. For first programming without ST-Link, follow
[`firmware-update.md`](firmware-update.md).

## Raspberry Pi Wi-Fi camera node

| Connection | Destination | Note |
| --- | --- | --- |
| IMX708 CSI ribbon | Raspberry Pi camera connector | Camera/video source |
| Regulated Pi 5 V supply | Raspberry Pi power input | Dedicated fused branch sized for Pi and camera |
| Wi-Fi | Operator access point/device | Video, logging and future IP control |
| Optional Wi-Fi/IP | ESP32 | Future autonomy requests and relayed telemetry |
| CAN-H / CAN-L | No connection | Raspberry Pi is not a runtime CAN node |

Do not run camera data through ESP32. Raspberry Pi encodes and streams video
directly over Wi-Fi. A Pi or Wi-Fi failure must not affect the XR4 to ESP32 to
STM32 manual-control path.

## CAN trunk

| Bus conductor | Color | Wiring rule |
| --- | --- | --- |
| CAN-H | Yellow | One continuous conductor, daisy-chain nodes |
| CAN-L | Green | Twist with CAN-H for the complete trunk |
| CAN reference ground | Black | Connect node signal grounds; do not carry motor current |
| Shield/drain, if used | Bare/clear | Bond according to the final enclosure grounding plan |

Use Classic CAN at 500 kbit/s. Install exactly two 120 ohm resistors between
CAN-H and CAN-L, one at each physical end. With power off, a resistance check
between CAN-H and CAN-L should be approximately 60 ohms when both terminators
are installed.

Only STM32 Left, ESP32, and STM32 Right are permanent CAN nodes, each with one
SN65HVD230. If that is also their physical order, enable 120 ohm termination at
Left and Right only. Raspberry Pi has no permanent CAN transceiver. A USB-CAN
adapter may be attached temporarily during firmware service.

## Power and grounding rules

- Battery/motor power, Raspberry Pi 5 V, ESP32/XR4 5 V, STM32/sensors, and
  lighting need appropriately rated protected branches.
- Put a fuse near the source of every branch; wire gauge follows measured
  current and cable length, not signal-color convention.
- Keep motor-current returns out of logic/sensor ground wires. Join references
  at the planned distribution point.
- Do not connect separate 3.3 V regulator outputs together.
- The physical emergency stop must disable motor-drive power or driver-enable
  independently of CAN and software while allowing the Pi to remain powered
  for logging when practical.
- Before first power-up, check continuity, polarity, CAN termination, and
  absence of shorts with the battery disconnected.
