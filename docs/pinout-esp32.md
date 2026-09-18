# ESP32 AUX pinout

Target: Sixspan ESP32-S3-N16R8, 16 MB flash and 8 MB octal PSRAM.

| Function | GPIO | Notes |
| --- | ---: | --- |
| Sensor I2C SDA / SCL | 1 / 2 | QMI8658A, compass, future light sensor |
| Encoder A / B / button | 4 / 5 / 6 | Local UI encoder |
| QMI8658A interrupt | 7 | Optional INT1 |
| OLED SDA / SCL | 8 / 9 | SH1106 at `0x3C` |
| Front / rear light | 10 / 11 | External drivers required |
| Left / right indicator | 12 / 13 | External drivers required |
| Buzzer | 14 | External driver when required |
| GPS TX / RX | 15 / 16 | 115200 baud |
| Native USB D- / D+ | 19 / 20 | Reserved |
| XR4 CRSF RX / TX | 21 / 38 | 420000 baud; XR4 TX→GPIO21 |
| **Left-link ESP TX / RX** | **39 / 40** | UART0, 115200; GPIO39→PA3, GPIO40←PA2 |
| **Right-link ESP TX / RX** | **41 / 42** | UART2, 115200; GPIO41→PA3, GPIO42←PA2 |
| ROM programming TX / RX | 43 / 44 | CH343/USB-UART, used before app remaps UART0 |
| Service / RGB LED | 47 / 48 | Optional / onboard RGB |

GPIO17 and GPIO18 are free after CAN removal. GPIO0, GPIO3, GPIO45, and
GPIO46 are boot-strapping pins and remain unallocated. GPIO26-GPIO37 are
excluded because of this board's flash/PSRAM routing or availability.

The left link uses UART0 at runtime. Application console output is disabled so
log text cannot corrupt motor commands. This does **not** prevent flashing:
the ESP32 ROM downloader still starts on the board's normal USB-UART pins
GPIO43/GPIO44 before application code runs.

All four motor-link pins are 3.3 V logic. Connect a common ground and cross
each UART: ESP TX to STM RX, STM TX to ESP RX. Do not connect SN65HVD230,
CAN-H/CAN-L, or termination resistors.
