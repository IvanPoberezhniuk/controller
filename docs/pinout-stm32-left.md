# STM32 Left target

The Left image controls front-left, center-left, and rear-left as motor0,
motor1, and motor2. Its private ESP32 link is USART2 at 115200 baud:

| STM32 pin | Connection |
| --- | --- |
| `PA3 / USART2_RX` | ESP32 `GPIO39 / LEFT_TX` (brown) |
| `PA2 / USART2_TX` | ESP32 `GPIO40 / LEFT_RX` (purple) |
| `GND` | ESP32 logic GND (black) |

The binary command includes the `LEFT` role and CRC. This firmware rejects
frames addressed to Right. PA11 and PA12 are now free; FDCAN is disabled.

Motor PWM, enables, encoders, and current-sense pins are shared with the Right
layout and documented in [wiring.md](wiring.md). Flash this board with
`UGV_STM32_LEFT.bin` using ST-Link.
