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
| Reserved network gateway | 0x20 |
| Reserved BMS gateway | 0x30 |
| ESP32 control/AUX | 0x40 |

## Messages

| CAN ID | DLC | Message | Producer | Consumers |
| ---: | ---: | --- | --- | --- |
| 0x100 | 8 | Final vehicle motion, signed RPM targets | ESP32 | Both STM32 nodes |
| 0x110 | 2 | Final enable / emergency stop | ESP32 | Both STM32 nodes |
| 0x130 | 8 | Active mode, source and CRSF link status | ESP32 | Optional CAN service monitor |
| 0x180 / 0x181 | 8 | Local three-wheel RPM telemetry | Left / Right STM32 | ESP32, optional service monitor |
| 0x190 / 0x191 | 8 | Per-motor fault masks | Left / Right STM32 | ESP32, optional service monitor |
| 0x710 / 0x711 | 8 | Motor-node heartbeat | Left / Right STM32 | ESP32, optional service monitor |
| 0x740 | 8 | Control/AUX-node heartbeat | ESP32 | Optional service monitor |

Raspberry Pi is not a CAN node. Future autonomy commands arrive at ESP32 over
Wi-Fi/IP, and ESP32 may relay selected CAN telemetry back to Pi over that same
network link.

The following identifiers remain in the checked-in header/DBC for compatibility
but have no runtime producer in the Wi-Fi-only Pi architecture. Do not build
new logic around them; they can be removed in a later wire-protocol version.

| CAN ID | Former purpose | Current status |
| ---: | --- | --- |
| 0x101 | Raspberry Pi autonomous motion request | Reserved; replaced by Pi-to-ESP32 Wi-Fi request |
| 0x111 | Raspberry Pi autonomous enable request | Reserved; replaced by Pi-to-ESP32 Wi-Fi request |
| 0x120 | Raspberry Pi auxiliary lighting request | Reserved; ESP32 owns local lighting |
| 0x720 | Raspberry Pi CAN gateway heartbeat | Reserved; Pi has no CAN interface |

## Firmware-update messages

Firmware update uses a small recovery protocol outside the operational DBC.
Its canonical definitions and byte codecs are in
`shared/update/ugv_fw_update_protocol.h`.

| CAN ID | DLC | Purpose | Direction |
| ---: | ---: | --- | --- |
| `0x600` | 8 | ENTER, QUERY, BEGIN, FINISH, ACTIVATE, ABORT | SocketCAN service host to selected STM32 |
| `0x610` | 8 | Sequence plus six Left image bytes | Service host to STM32 Left |
| `0x611` | 8 | Sequence plus six Right image bytes | Service host to STM32 Right |
| `0x680` | 8 | Left READY/ACK/VERIFIED/ERROR | STM32 Left to service host |
| `0x681` | 8 | Right READY/ACK/VERIFIED/ERROR | STM32 Right to service host |

The updater acknowledges every 32 data frames, resumes from the next expected
sequence after a timeout, and verifies CRC-32 before activation. Normal motion
commands remain unavailable while a node is in its bootloader. See
[`firmware-update.md`](firmware-update.md) for the state flow and recovery
procedure.

All receivers reject an unexpected DLC and invalid bounded fields. Motion
commands carry a sequence counter and must arrive every 10-20 ms. The ESP32 is
the only producer allowed to use the final command IDs `0x100` and `0x110`.
Raspberry Pi cannot compete for those identifiers because it is physically
absent from CAN.

The RC link timeout is 100 ms, the autonomous-request timeout is 300 ms, and
the motor-node final-command timeout is 300 ms. Loss of the source selected by
the operator causes a stop; it never automatically switches MANUAL/AUTO mode.
Adding the FDCAN/TWAI transports must preserve the existing safe target reset
and driver-disable behavior.

An individual six-wheel target frame is intentionally not assigned yet: six
signed 16-bit RPM values require 12 data bytes and do not fit in one Classic
CAN frame. Define a split-frame or scaled representation before adding it.
