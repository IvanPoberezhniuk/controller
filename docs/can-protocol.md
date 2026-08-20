# CAN protocol

Classic CAN runs at 500 kbit/s. `shared/can/ugv_can_protocol.h` is the wire
contract for message IDs, DLCs, units, payload types, and timeout constants.
`ugv_can_codec.c` performs explicit little-endian serialization. `ugv.dbc`
mirrors the same contract for CAN tooling; `Tests/can/test_dbc_sync.ps1`
prevents its message table from drifting from the header.

## Node IDs

| Node | ID |
| --- | ---: |
| STM32 Left | 0x10 |
| STM32 Right | 0x11 |
| Raspberry Pi gateway | 0x20 |
| Reserved BMS gateway | 0x30 |
| ESP32 AUX | 0x40 |

## Messages

| CAN ID | DLC | Message | Producer | Consumers |
| ---: | ---: | --- | --- | --- |
| 0x100 | 8 | Vehicle motion, signed RPM targets | Raspberry Pi | Both STM32 nodes |
| 0x110 | 2 | Enable / emergency stop | Raspberry Pi | All control nodes |
| 0x120 | 4 | Auxiliary lighting | Raspberry Pi | ESP32 AUX |
| 0x180 / 0x181 | 8 | Local three-wheel RPM telemetry | Left / Right STM32 | Raspberry Pi, ESP32 |
| 0x190 / 0x191 | 8 | Per-motor fault masks | Left / Right STM32 | Raspberry Pi, ESP32 |
| 0x1A0 / 0x1A1 | 8 | Temperatures with validity mask | Left / Right STM32 | Raspberry Pi, ESP32 |
| 0x710 / 0x711 | 8 | Motor-node heartbeat | Left / Right STM32 | Raspberry Pi |
| 0x720 | 8 | Gateway heartbeat | Raspberry Pi | STM32 nodes, ESP32 |
| 0x740 | 8 | Auxiliary-node heartbeat | ESP32 AUX | Raspberry Pi |

All receivers reject an unexpected DLC and invalid bounded fields. Motion
commands carry a sequence counter and must arrive every 10-20 ms. The current
motor-node command timeout is 300 ms; adding the FDCAN transport must preserve
the existing safe target reset and driver-disable behavior.

An individual six-wheel target frame is intentionally not assigned yet: six
signed 16-bit RPM values require 12 data bytes and do not fit in one Classic
CAN frame. Define a split-frame or scaled representation before adding it.
