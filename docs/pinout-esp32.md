# ESP32 AUX pinout

Target: Sixspan ESP32-S3-N16R8, 16 MB flash and 8 MB octal PSRAM.

| Function | GPIO | Notes |
| --- | ---: | --- |
| Sensor I2C SDA | 1 | QMI8658A, M100-5883 compass, future ambient-light sensor |
| Sensor I2C SCL | 2 | Shared 100 kHz sensor bus; separate from OLED |
| Encoder A | 4 | Existing proven assignment |
| Encoder B | 5 | Existing proven assignment |
| Encoder button | 6 | Input with pull-up |
| QMI8658A interrupt | 7 | Optional INT1/data-ready input |
| OLED SDA | 8 | SH1106, address 0x3C |
| OLED SCL | 9 | I2C, 400 kHz |
| Front light PWM | 10 | External MOSFET/driver required |
| Rear light PWM | 11 | External MOSFET/driver required |
| Left indicator | 12 | External MOSFET/driver required |
| Right indicator | 13 | External MOSFET/driver required |
| Buzzer | 14 | Driver transistor if current requires it |
| GPS UART TX | 15 | ESP TX to M100-5883 RX, 115200 baud |
| GPS UART RX | 16 | ESP RX from M100-5883 TX, 115200 baud |
| TWAI TX | 17 | To 3.3 V CAN transceiver TXD |
| TWAI RX | 18 | From CAN transceiver RXD |
| XR4 CRSF RX | 21 | From RadioMaster XR4 TX |
| XR4 CRSF TX | 38 | To RadioMaster XR4 RX for telemetry |
| Native USB D- / D+ | 19 / 20 | Reserved for USB |
| UART0 TX / RX | 43 / 44 | Reserved for programming/logging |
| Service LED | 47 | Optional |
| Onboard RGB LED | 48 | Addressable RGB status LED |

GPIO39-GPIO42 remain free. GPIO0, GPIO3, GPIO45, and GPIO46 are boot-strapping
pins and are deliberately not allocated. GPIO26-GPIO37 are excluded from the
board plan because they are associated with the module's flash/PSRAM interface
or are unavailable on this board variant.

The XR4 uses its primary, non-inverted, full-duplex CRSF port at 420000 baud.
Cross-connect UART signals: XR4 TX to ESP32 GPIO21 and XR4 RX to ESP32 GPIO38.
Power the receiver from a regulated 5 V rail, never from the ESP32 3.3 V pin,
and connect receiver and ESP32 grounds.

The OLED must be powered from 3.3 V even though the module specification also
allows 5 V. Its known configuration is SH1106, 128x64, I2C address `0x3C`.

Power QMI8658A from 3.3 V. Scan for `0x6A` and `0x6B`; register `0x00` must
return `0x05`. Power HGLRC M100-5883 from the regulated 5 V sensor rail. Its
GPS uses GPIO15/16 UART while its integrated compass separately uses GPIO1/2
I2C. The compass is expected at `0x0D`, but the fitted 5883 variant must be
confirmed by an I2C scan on the actual module.
