---
name: ugv-can-protocol
description: TRIGGER when discussing the UGV's CAN bus — FDCAN transceiver/physical layer, node/message IDs and payload layout, bit rate, or heartbeat/command-timeout behavior between the Raspberry Pi and STM32 nodes.
---

# UGV CAN architecture

## Physical layer

STM32G431 includes an FDCAN controller but **not** a physical transceiver —
an external transceiver is mandatory. Previously available/discussed:
SN65HVD230.

Requirements: twisted pair for CANH/CANL; 120 ohm termination at exactly both
physical ends of the bus; short stubs; common signal ground between nodes;
suitable locking connectors; ESD protection where appropriate; decoupling
directly at every transceiver; avoid routing CAN alongside noisy motor leads
for long distances; use a linear bus, not a star, unless a tested active hub
is used.

Suggested initial bit rate: 500 kbit/s classic CAN. CAN FD can be used later,
but classic CAN is sufficient for initial motor commands and telemetry.

## Network participants

Initial nodes: Raspberry Pi CAN interface/gateway, left STM32G431 motor
controller, right STM32G431 motor controller.

Potential future nodes: battery/BMS gateway, lighting controller, robotic
arm, turret, suspension controller, detachable wheel modules, sensor module.

## Command principles

Every motion command should contain: command sequence counter, drive mode,
enable state, left/right targets or six individual wheel targets,
emergency-stop status, flags, optional command timestamp/age.

Motor nodes must reject: malformed frames, invalid length, invalid mode,
impossible target values, stale counters, out-of-range values.

## Example messages and IDs

```text
0x100 — vehicle motion command
0x110 — system enable / emergency stop
0x120 — auxiliary or lighting command

0x180 — left-node fast telemetry
0x181 — right-node fast telemetry

0x190 — left-node fault report
0x191 — right-node fault report

0x1A0 — left-node temperatures/current
0x1A1 — right-node temperatures/current

0x710 / 0x711 — left/right STM32 heartbeat
0x720 — Raspberry Pi gateway heartbeat
0x740 — ESP32 AUX heartbeat
```

Example vehicle command payload:

```text
Byte 0: sequence counter
Byte 1: mode and flags
Byte 2-3: signed left target
Byte 4-5: signed right target
Byte 6: speed or torque limit
Byte 7: checksum or reserved field
```

`shared/can/ugv_can_protocol.h` is the wire contract shared by STM32 firmware,
ESP32, Raspberry Pi services, PC tooling, and tests. `ugv.dbc` mirrors it for
CAN tools; run `Tests/can/test_dbc_sync.ps1` after protocol edits.

Do not assign a six-wheel `int16` target frame until its Classic CAN encoding
is designed: six values require 12 bytes and cannot fit in one frame.

## Heartbeat and timeout

Raspberry Pi sends motion commands at 50-100 Hz. The current motor-node
timeout is 300 ms and is declared centrally in `ugv_can_protocol.h`; tune it
only after measuring real bus and control-path latency.

**The STM32 must not continue using the last speed command indefinitely.**
