---
name: ugv-can-protocol
description: TRIGGER when discussing the UGV's CAN bus — SN65HVD230/FDCAN/TWAI physical layer, node/message IDs and payload layout, bit rate, or heartbeat/command-timeout behavior between ESP32 and the STM32 nodes.
---

# UGV CAN architecture

## Physical layer

STM32G431 includes an FDCAN controller but **not** a physical transceiver —
an external transceiver is mandatory. The confirmed available transceiver is
SN65HVD230, powered from 3.3 V. ESP32 uses another SN65HVD230 with TWAI.

Requirements: twisted pair for CANH/CANL; 120 ohm termination at exactly both
physical ends of the bus; short stubs; common signal ground between nodes;
suitable locking connectors; ESD protection where appropriate; decoupling
directly at every transceiver; avoid routing CAN alongside noisy motor leads
for long distances; use a linear bus, not a star, unless a tested active hub
is used.

Suggested initial bit rate: 500 kbit/s classic CAN. CAN FD can be used later,
but classic CAN is sufficient for initial motor commands and telemetry.

## Network participants

Permanent nodes: ESP32 command/AUX node, left STM32G431 motor controller, and
right STM32G431 motor controller. Raspberry Pi is Wi-Fi-only. A SocketCAN
service adapter may join temporarily for bootloader maintenance.

Potential future nodes: battery/BMS gateway, lighting controller, robotic
arm, turret, suspension controller, detachable wheel modules, sensor module.

## Command principles

The final motion command contains a sequence counter, drive-mode flags, signed
left/right RPM targets, and a percentage limit. Enable/emergency stop uses a
separate frame. Individual six-wheel targets remain an unresolved encoding.

Only ESP32 produces final command IDs 0x100/0x110. Raspberry Pi sends future
AUTO requests to ESP32 over Wi-Fi/IP, and ESP32 explicitly arbitrates MANUAL
CRSF versus AUTO. Loss of the selected source stops the vehicle; never
auto-switch modes.

Motor nodes must reject: malformed frames, invalid length, invalid mode,
impossible target values, stale counters, out-of-range values.

## Example messages and IDs

```text
0x100 — vehicle motion command
0x101 — reserved former Raspberry Pi CAN motion request
0x110 — system enable / emergency stop
0x111 — reserved former Raspberry Pi CAN enable request
0x120 — reserved former Raspberry Pi auxiliary request
0x130 — ESP32 control-source and CRSF-link status

0x180 — left-node fast telemetry
0x181 — right-node fast telemetry

0x190 — left-node fault report
0x191 — right-node fault report

0x710 / 0x711 — left/right STM32 heartbeat
0x720 — reserved former Raspberry Pi CAN heartbeat
0x740 — ESP32 AUX heartbeat
```

Example vehicle command payload:

```text
Byte 0: sequence counter
Byte 1: mode and flags
Byte 2-3: signed left target RPM
Byte 4-5: signed right target RPM
Byte 6: command limit, 0-100 percent
Byte 7: reserved
```

`shared/can/ugv_can_protocol.h` is the wire contract shared by STM32 firmware,
ESP32, service tooling, and tests. Raspberry Pi uses a separate Wi-Fi/IP
contract. `ugv.dbc` mirrors CAN for tools; run
`Tests/can/test_dbc_sync.ps1` after protocol edits.

Do not assign a six-wheel `int16` target frame until its Classic CAN encoding
is designed: six values require 12 bytes and cannot fit in one frame.

## Heartbeat and timeout

ESP32 sends final motion commands at 50-100 Hz. The RC timeout is 100 ms; the
future Pi Wi-Fi AUTO-request and motor-node final-command timeouts are 300 ms.
Values are declared centrally in `ugv_can_protocol.h`; tune only after
measurement.

**The STM32 must not continue using the last speed command indefinitely.**
