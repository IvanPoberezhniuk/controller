# UGV wiring diagram

This file is the harness reference for the current architecture. Label both
ends of every wire with the signal name; color alone is not an identifier.

`FINAL` means the pin is represented in the checked-in source and CubeMX
configuration. It does not replace electrical validation: keep motor power
disconnected until the harness has been continuity-checked.

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

Keep the XR4 supply at a regulated `5.00 V`. Its specified `4.5-8.4 V` input
range is an allowed operating range, not a telemetry-power adjustment. Raising
the input toward 8.4 V does not increase the specified 100 mW telemetry output
and only reduces transient margin and increases dissipation in the receiver's
internal power circuitry. Fit 47-100 uF bulk capacitance plus 100 nF ceramic
decoupling close to the XR4 `5V/GND` pads, and verify the voltage there while
telemetry and receiver Wi-Fi are active.

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

### Shared 3.3 V and central 5 V supplies

Use one MP1584EN adjusted to `3.30 V` for every 3.3 V consumer in the control
system. Use a second MP1584EN adjusted to `5.00 V` for XR4 and M100-5883.

```text
Protected DC logic branch
          |
          +--> MP1584EN #1: 3.30 V, 2 A / 6.6 W design allocation
          |       +--> fused Central branch: ESP32 and central 3V3 peripherals
          |       +--> fused Left branch: STM32, CAN, 3 encoders, 3 IBT-2 logic
          |       `--> fused Right branch: STM32, CAN, 3 encoders, 3 IBT-2 logic
          |
          `--> MP1584EN #2: 5.00 V
                  +--> XR4 receiver
                  `--> M100-5883 GPS/compass
```

| MP1584EN terminal | Connection | Wire identification |
| --- | --- | --- |
| `IN+` | Fused/protected DC logic-branch positive | Red, label with input voltage |
| `IN-` | Power-distribution ground | Black, label `GND` |
| `OUT+` on #1 | Shared star-distributed `3V3` rail | Red, label `3V3` |
| `OUT+` on #2 | Central `5V` rail | Red, label `5V` |
| `OUT-` | Common logic ground | Black, label `GND` |

The shared 3.3 V design allocation is `2 A / 6.6 W`, below the module's stated
10 W ceiling but high enough that enclosure temperature and remote-node
voltage drop must be tested. Run separate positive and ground conductors from
the distribution point to Central, Left, and Right; do not daisy-chain node
power. If the module overheats or either remote rail sags during motor or radio
activity, replace the shared arrangement with local converters.

The ESP32-S3 alone requires a supply designed for at least `500 mA`; the
Central branch reserves `1 A`. Before direct connection, confirm that the exact
Sixspan board permits its `3V3` pin to be used as a power input and isolate its
onboard 3.3 V regulator output. Do not also feed the board through `5V/VIN` or
powered USB. For programming, isolate USB VBUS or use a debugger that does not
source target power.

Both MP1584EN modules are step-down converters. Their inputs must remain within
`4.5-28 V`, including battery-charge voltage and transients. Adjust each module
with all output loads disconnected, power-cycle it, and verify `3.30 V` or
`5.00 V` again before connecting electronics. Secure the trimmers after setup.

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

### Shared 3.3 V branch at each STM32 node

Each motor node receives a separate branch from the shared 3.3 V star point.
One node reserves `0.5 A / 1.65 W` for its STM32 board, SN65HVD230, three motor
encoders, and three IBT-2 logic interfaces. Add local bulk decoupling at the
node power entrance and 100 nF at each IC supply pair. Verify 3.3 V at the node
while all three motors switch and while CAN traffic is active.

### BTS7960/IBT-2 power domains

Each driver needs motor power and logic power, but it does not need its own
3.3 V converter.

| IBT-2 connection | Supply/source | Rule |
| --- | --- | --- |
| Header `VCC` | Shared regulated `3V3` branch | Logic power only; applies to the pictured `AHC244D` module |
| Header `GND` | Local logic ground | Common reference with STM32 and encoder ground |
| Screw terminal `B+` | Protected motor-battery positive | High-current supply; individual driver fuse recommended |
| Screw terminal `B-` | Motor-battery negative distribution | High-current return; never route through STM32 ground wiring |
| Screw terminal `M+` | Motor red factory lead | H-bridge output, not a power input |
| Screw terminal `M-` | Motor white factory lead | H-bridge output, not a power input |

The six drivers may share the motor-battery distribution bus, but each driver
should have its own correctly sized fuse for fault isolation. STM32 GPIOs drive
only `RPWM`, `LPWM`, `R_EN`, and `L_EN`; they never supply driver power.

### Factory motor and encoder leads

The supplied GB37-520B motors have six factory-colored leads:

| Factory lead | Function | Connection |
| --- | --- | --- |
| Red | Motor terminal + | That motor's BTS7960 `M+` output |
| White | Motor terminal - | That motor's BTS7960 `M-` output |
| Blue | Encoder supply + | Regulated `3V3` |
| Black | Encoder supply - | STM32 signal `GND` |
| Yellow | Encoder channel A | The motor's STM32 encoder-A input below |
| Green | Encoder channel B | The motor's STM32 encoder-B input below |

Red/white polarity defines the initial direction convention only; the H-bridge
reverses the motor by reversing those terminals. Never connect either motor
lead directly to the STM32. Power the encoder from 3.3 V so its output-high
level remains safe for the STM32 inputs.

The motor's blue encoder-supply wire and the pictured IBT-2 module's `VCC`
pin are two separate loads connected to the shared regulated `3V3`
rail. The motor's black encoder-ground wire, STM32 `GND`, and every IBT-2
`GND` must join the same common logic-ground net. This 3.3 V IBT-2 logic supply
applies to the pictured module fitted with an `AHC244D` buffer; verify that
marking before connection because visually similar modules can differ.

All six factory motor wires are accounted for above. The motor cable has no
orange wire and does not need one.

#### Factory encoder signals to STM32

| Motor | Factory lead | Function | STM32 pin | Status |
| --- | --- | --- | --- | --- |
| Front / motor0 | Yellow | Encoder A | `PA0` | FINAL |
| Front / motor0 | Green | Encoder B | `PA1` | FINAL |
| Center / motor1 | Yellow | Encoder A | `PB4` | FINAL |
| Center / motor1 | Green | Encoder B | `PB5` | FINAL |
| Rear / motor2 | Yellow | Encoder A | `PB6` | FINAL |
| Rear / motor2 | Green | Encoder B | `PB7` | FINAL |

Retain the factory colors and label both ends of every lead with its signal
name. Before installing all six motors, verify one motor against its supplied
encoder-wiring sheet and bench-test both encoder channels at 3.3 V. Stop if the
observed pinout or output voltage differs from the table above.

### STM32 to BTS7960 control wiring

The wires below are separate Arduino/Dupont jumpers added between each STM32
board and its three BTS7960 driver modules. They are not part of the motor's
six-wire factory cable. The listed colors are only the chosen harness
convention; another available color is electrically equivalent if both ends
are labeled with the signal name.

For each pictured `AHC244D`-equipped IBT-2 module, also connect STM32 `3V3` to
the module header `VCC` and connect STM32 `GND` to module header `GND`.

| Motor | From STM32 | To BTS7960 | Suggested added jumper | Status |
| --- | --- | --- | --- | --- |
| Front / motor0 | `PA8` | `RPWM` | Orange | FINAL |
| Front / motor0 | `PA9` | `LPWM` | Yellow | FINAL |
| Front / motor0 | `PB0` | `R_EN` + `L_EN`, tied together | Green | FINAL common enable |
| Center / motor1 | `PA10` | `RPWM` | Orange | FINAL |
| Center / motor1 | `PB8` (`TIM16_CH1`) | `LPWM` | Yellow | FINAL |
| Center / motor1 | `PB9` | `R_EN` + `L_EN`, tied together | Green | FINAL common enable |
| Rear / motor2 | `PB14` | `RPWM` | Orange | FINAL |
| Rear / motor2 | `PB15` | `LPWM` | Yellow | FINAL |
| Rear / motor2 | `PB10` | `R_EN` + `L_EN`, tied together | Green | FINAL common enable |

Connect both enable inputs of each driver to its single common-enable GPIO.
Add one 10 kohm pull-down from each common-enable net to GND so all drivers
stay disabled while the STM32 is resetting or unpowered. Do not join enable
nets between different motors.

### Direct current-sense ADC wiring

| Motor | Driver signal | STM32 pin / ADC channel | Color | Status |
| --- | --- | --- | --- | --- |
| Front / motor0 | `R_IS` | `PA6 / ADC2_IN3` | White | FINAL |
| Front / motor0 | `L_IS` | `PA7 / ADC2_IN4` | Gray | FINAL |
| Center / motor1 | `R_IS` | `PA4 / ADC2_IN17` | White | FINAL |
| Center / motor1 | `L_IS` | `PA5 / ADC2_IN13` | Gray | FINAL |
| Rear / motor2 | `R_IS` | `PB2 / ADC2_IN12` | White | FINAL |
| Rear / motor2 | `L_IS` | `PB12 / ADC1_IN11` | Gray | FINAL |

There is no CD74HC4067 in the final harness. Every R_IS/L_IS signal connects
to its own ADC pin and must be conditioned so its complete voltage range stays
within `0-3.3 V`. Add the divider/buffer, RC filtering, and input protection
required by the selected motor-driver module. Never connect an unverified 5 V
sense output directly to the STM32.

`PB1`, `PB11`, and `PB13` are free GPIO reserve. Leave them unconnected until a
future function is deliberately added to both the schematic and CubeMX file.

### STM32 SN65HVD230 CAN transceiver

| From | To | Color | Status / note |
| --- | --- | --- | --- |
| STM32 `PA12 / FDCAN1_TX` | SN65HVD230 `D/TXD` | Orange | FINAL; 10 kohm pull-up to 3V3 |
| SN65HVD230 `R/RXD` | STM32 `PA11 / FDCAN1_RX` | White | FINAL |
| STM32 `3V3` | SN65HVD230 `VCC` | Red, label `3V3` | Never connect to 5 V |
| STM32 `GND` | Transceiver `GND` | Black | Common signal reference |
| Transceiver `CANH` | CAN-H trunk | Yellow | Twisted with CAN-L |
| Transceiver `CANL` | CAN-L trunk | Green | Twisted with CAN-H |
| SN65HVD230 `RS` | `GND` | Black | High-speed mode; often handled on modules |
| SN65HVD230 `Vref` | Not connected | No wire | Leave floating when unused |

PA11/PA12 connect only to the transceiver logic pins, never directly to
CAN-H/CAN-L. Place 100 nF at VCC/GND and SM24CANB at the bus connector. The
bootloader and application both configure these pins.

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

Planned MP1584EN count:

| Quantity | Setting | Power domain |
| --- | --- | --- |
| 1 | `3.30 V` | All shared 3.3 V loads: ESP32, both STM32 nodes, CAN, sensors, encoders, and IBT-2 logic |
| 1 | `5.00 V` | XR4 and M100-5883 |

The normal plan therefore uses two MP1584EN modules. Raspberry Pi 5, motor
power, and lighting do not use these modules. See the detailed
[low-voltage power budget](power-budget.md) for per-load allocations and
measurement requirements.

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
