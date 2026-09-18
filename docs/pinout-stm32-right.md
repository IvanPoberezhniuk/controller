# STM32 Right target

The Right image controls front-right, center-right, and rear-right as motor0,
motor1, and motor2. Its private ESP32 link is USART2 at 115200 baud:

| STM32 pin | Connection |
| --- | --- |
| `PA3 / USART2_RX` | ESP32 `GPIO41 / RIGHT_TX` (brown) |
| `PA2 / USART2_TX` | ESP32 `GPIO42 / RIGHT_RX` (purple) |
| `GND` | ESP32 logic GND (black) |

The binary command includes the `RIGHT` role and CRC. This firmware rejects
frames addressed to Left. PA11 and PA12 are now free; FDCAN is disabled.

Motor PWM, enables, encoders, and current-sense pins are shared with the Left
layout and documented in [wiring.md](wiring.md). Flash this board with
`UGV_STM32_RIGHT.bin` using ST-Link.
