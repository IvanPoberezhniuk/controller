# CAN protocol

Classic CAN runs at 500 kbit/s. `shared/can/ugv.dbc` is the machine-readable
description and `ugv_can_protocol.h` defines IDs and semantic payload types.
`ugv_can_codec.c` is the canonical C wire encoder/decoder.

## Node IDs

| Node | ID |
| --- | ---: |
| STM32 Left | 0x10 |
| STM32 Right | 0x11 |
| Raspberry Pi gateway | 0x20 |
| Future BMS gateway | 0x30 |
| ESP32 AUX | 0x40 |

## Initial messages

| CAN ID | Message | Producer | Consumers |
| ---: | --- | --- | --- |
| 0x100 | Vehicle motion | Raspberry Pi | Both STM32 nodes |
| 0x110 | Enable / emergency stop | Raspberry Pi | All control nodes |
| 0x120 | Auxiliary lighting | Raspberry Pi | ESP32 AUX |
| 0x180 / 0x181 | Motor telemetry | Left / Right STM32 | Raspberry Pi, ESP32 |
| 0x190 / 0x191 | Motor faults | Left / Right STM32 | Raspberry Pi, ESP32 |
| 0x1A0 / 0x1A1 | Motor temperatures | Left / Right STM32 | Raspberry Pi, ESP32 |
| 0x700 + node ID | Heartbeat | Every node | Raspberry Pi and peers |

All multi-byte values are little-endian. Receivers must reject frames with an
unexpected DLC. Motion commands use a sequence counter and motor nodes apply
the timeout thresholds declared in `ugv_can_protocol.h`.
