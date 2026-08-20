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
                                         CAN transceiver
                                               |
 CAN end A -- Raspberry Pi -- STM32 Left -- ESP32 -- STM32 Right -- CAN end B
               transceiver     transceiver              transceiver
                                               |
                                  exactly two 120 ohm terminators:
                                  one at end A, one at end B
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

### CAN transceiver

| From | To | Color | Status / note |
| --- | --- | --- | --- |
| ESP32 `GPIO17` (`TWAI_TX`) | Transceiver `TXD` | Orange | FINAL; logic-side signal |
| Transceiver `RXD` | ESP32 `GPIO18` (`TWAI_RX`) | White | FINAL; logic-side signal |
| ESP32 `3V3` | Transceiver `VCC/VIO` | Red, label `3V3` | Use a 3.3 V-compatible transceiver |
| ESP32 `GND` | Transceiver `GND` | Black | Common signal reference |
| Transceiver `CANH` | CAN-H trunk | Yellow | Twisted pair |
| Transceiver `CANL` | CAN-L trunk | Green | Twisted pair |

The transceiver and its required decoupling/ESD parts belong close to the
ESP32. ESP32 GPIOs must never connect directly to CAN-H/CAN-L.

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

### IMU and ambient-light sensor bus

Both devices share this I2C bus. Give each module a unique address.

| Device pin | ESP32 | Color | Status / note |
| --- | --- | --- | --- |
| IMU/light `VCC` | `3V3` | Red, label `3V3` | Confirm exact modules before power-up |
| IMU/light `GND` | `GND` | Black | FINAL |
| IMU/light `SDA` | `GPIO1` | Blue | FINAL |
| IMU/light `SCL` | `GPIO2` | Yellow | FINAL |
| IMU `INT/DRDY` | `GPIO7` | White | FINAL but optional |

Install only one effective SDA pull-up and one SCL pull-up set for the complete
bus. Parallel pull-ups already fitted to breakout boards can become too strong.

### GPS UART

The GPS model and its supply voltage are not finalized. Do not connect its
power until the module label/datasheet is checked.

| GPS pin | ESP32 | Color | Status / note |
| --- | --- | --- | --- |
| GPS `TX` | `GPIO16` (`GPS_RX`) | White | FINAL; UART signals cross |
| GPS `RX` | `GPIO15` (`GPS_TX`) | Orange | FINAL |
| GPS `GND` | `GND` | Black | FINAL |
| GPS `VCC` | Module-rated rail | Red, label voltage | TBD by exact GPS module |

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
| `C6` | Front motor temperature | Violet |
| `C7` | Center motor temperature | Violet |
| `C8` | Rear motor temperature | Violet |

MUX `VCC` goes to STM32 `3V3` with a red wire labelled `3V3`; MUX `GND` and
`EN` go to ground with black wires so the chip is always enabled. Place a
100 nF decoupling capacitor directly between MUX VCC and GND.

Every analog source connected to `C0-C8` must be conditioned so its complete
voltage range stays within `0-3.3 V`. Add dividers, buffers, and input
protection as required by the final motor driver and temperature sensor; the
multiplexer itself does not make a 5 V signal safe for the STM32 ADC.

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

### STM32 CAN transceiver

| From | To | Color | Status / note |
| --- | --- | --- | --- |
| STM32 `PA12 / FDCAN1_TX` | Transceiver `TXD` | Orange | PLANNED CubeMX |
| Transceiver `RXD` | STM32 `PA11 / FDCAN1_RX` | White | PLANNED CubeMX |
| STM32 `3V3` | Transceiver `VCC/VIO` | Red, label `3V3` | Use 3.3 V-compatible logic |
| STM32 `GND` | Transceiver `GND` | Black | Common signal reference |
| Transceiver `CANH` | CAN-H trunk | Yellow | Twisted with CAN-L |
| Transceiver `CANL` | CAN-L trunk | Green | Twisted with CAN-H |

PA11/PA12 connect only to the transceiver logic pins, never directly to
CAN-H/CAN-L. The bootloader already configures these pins; the application
starts using them after the manual CubeMX migration.

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

Raspberry Pi, both STM32 boards, and ESP32 each need their own transceiver.
The Raspberry Pi CAN HAT/adapter model and its GPIO/SPI mapping are not yet
final, so that connector remains TBD rather than assigning imaginary Pi pins.

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
