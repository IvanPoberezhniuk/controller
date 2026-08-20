---
name: ugv-raspberry-pi
description: TRIGGER when discussing the UGV's Raspberry Pi 5 responsibilities or services — the CAN gateway, telemetry aggregation, systemd service structure, or Pi-side high-level architecture (not motor PID, which lives on STM32).
---

# UGV Raspberry Pi 5

## Confirmed hardware

Raspberry Pi 5, 4 GB RAM, hostname `roverpi` (reachable via mDNS as
`roverpi.local`), intended for headless operation, SSH enabled, Raspberry Pi
Connect used, Raspberry Pi OS 64-bit, camera connected through a 22-pin FFC
cable. Kernel/OS details are changeable — treat as current snapshot, not
fixed.

## Pi responsibilities

The Pi performs: video capture/compression, network communication, PC
control-station communication, high-level command processing, telemetry
aggregation, system configuration, logging, future navigation/computer
vision, optional ROS 2 integration, updating commands sent to STM32 nodes,
presenting faults to the operator, optional coordination of lights/auxiliary
modules.

**The Pi must not directly perform timing-critical wheel PID control** —
Linux scheduling delays must not be able to produce uncontrolled motor
behavior. That responsibility stays on STM32 (`ugv-stm32-firmware`).

## Suggested service structure

```text
ugv-control-gateway
- receives operator commands;
- validates commands;
- converts commands to CAN;
- aggregates CAN telemetry;
- manages heartbeats.

ugv-camera
- captures the camera;
- encodes H.264;
- publishes video stream.

ugv-telemetry
- stores logs;
- exposes telemetry to the PC/UI;
- records faults and events.

ugv-supervisor
- monitors services;
- manages startup order;
- reports health.
```

Systemd services should restart non-critical processes after failure.
**Motor safety must remain within STM32 firmware, not systemd.**
