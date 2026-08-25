# 6x6 UGV Controller Project

## What this project is

A functional, modular 6x6 skid-steer unmanned ground vehicle: six independently
driven wheels, closed-loop wheel-speed control, configurable 2WD/4WD/6WD modes,
Raspberry Pi 5 high-level computer, two STM32G431CBT6 deterministic low-level
motor controllers over CAN, live camera video, remote driving, telemetry, and
fail-safe behavior on communication loss.

It is also an embedded-engineering learning platform: implementations should be
understandable, testable, and documented rather than opaque shortcuts.

The initial target is a reliable manually controlled rover. Autonomous
navigation, ROS 2, computer vision, robotic arms, and turrets are later phases
— do not pull them forward without being asked.

## How to treat information in this project

For every proposed implementation, distinguish between:

1. confirmed hardware or measured behavior;
2. current design decision;
3. recommended implementation;
4. unresolved decision;
5. future expansion.

Do not silently invent missing hardware specs or treat tentative ideas as
final decisions. When information conflicts, the newest project decision
wins. Before presenting a hardware/protocol choice as final, cross-check it
against the unresolved-decisions and rejected-approaches registers in the
`ugv-project-plan` skill.

## Confirmed target architecture

```text
Radio + Nomad -> XR4 --CRSF--> ESP32-S3 control/AUX <--Wi-Fi/IP--> Raspberry Pi 5
                                MANUAL/AUTO arbiter               autonomy/logs
                                OLED/IMU/GPS/lights               camera/audio
                                         |
                                  final CAN commands
                                         |
                           +-------------+-------------+
                           |                           |
                           v                           v
               STM32G431CBT6 left          STM32G431CBT6 right
               3 encoders / PID loops      3 encoders / PID loops
               3 motor drivers             3 motor drivers
               CD74HC4067                   CD74HC4067
               local failsafe              local failsafe
                           |                           |
                           v                           v
               front/center/rear left       front/center/rear right

Motors:
6 x JGB37-520 brushed DC gearmotors
333 RPM reduction version

Battery:
4S LiFePO4 -> JK 60 A BMS -> main fuse -> distribution

Distribution:
- motor rail;
- Raspberry Pi DC/DC;
- STM32/sensor rail;
- lighting rail;
- cooling/auxiliary rail.

Camera:
IMX708 -> Raspberry Pi -> H.264 -> MediaMTX ->
rtsp://roverpi.local:8554/ugv -> Qt control station

Firmware repository:
- one shared STM32 motor-node implementation;
- separate left/right target configurations and build images;
- an independent ESP-IDF project for ESP32-S3 AUX;
- one shared, explicitly encoded CAN protocol for ESP32 and both STM32 nodes;
- a separate future Wi-Fi/IP protocol between ESP32 and Raspberry Pi.
```

## Agent response rules

1. Preserve the confirmed two-node STM32G431CBT6 architecture (one node per
   side, three motors each).
2. Treat the six 333 RPM reduction JGB37-520 motors as confirmed hardware.
3. Use measured data before theoretical worst-case assumptions.
4. Show calculations with units. State assumptions explicitly.
5. Never describe an unresolved component as purchased or finalized.
6. Prioritize safe bench validation before vehicle testing.
7. Keep motor-control safety on STM32; CRSF arbitration, final CAN command
   authority, auxiliary UI/sensors/lighting on ESP32; and video/networking/
   logging/autonomy requests on Raspberry Pi. Raspberry Pi is Wi-Fi-only and
   must not be added to the permanent CAN trunk.
8. Keep the CAN wire contract in `shared/can/ugv_can_protocol.h` and verify
   its DBC mirror with `Tests/can/test_dbc_sync.ps1`.
9. Include failure behavior in every implementation.
10. Avoid oversized hardware without explaining the tradeoff, and avoid
    undersized hardware based only on the single 3.6 A stall measurement.
11. Make firmware modular and testable — do not put the entire embedded
    program in `main.c`.
12. Never allow a reset or communication loss to energize motors, or to keep
    driving the last motor command indefinitely.
13. Prefer reproducible measurements and logs over estimates.
14. For each new feature, specify: hardware interface, power requirement,
    firmware responsibility, CAN messages, telemetry, failure mode, and
    validation procedure.

Immediate implementation priority:

```text
Validate one motor and encoder
-> select final H-bridge
-> control one motor using STM32G431CBT6
-> expand to three motors and local CD74HC4067 sensing
-> add CAN
-> validate separate left/right builds from the shared node firmware
-> bring up the ESP32 AUX node
-> integrate Raspberry Pi camera/video over Wi-Fi
-> integrate control station
-> perform complete electrical and safety testing
```

## Subsystem skills (loaded on demand)

Detailed specs, requirements, and open questions for each subsystem live in
skills under `.claude/skills/`, each triggered automatically when its topic
comes up:

- `ugv-mechanical` — chassis layout, dimensions, materials, printer/suspension constraints.
- `ugv-drivetrain` — motors, wheels, measured current, 2WD/4WD/6WD modes, steering mixing.
- `ugv-stm32-firmware` — STM32 hardware/peripherals, node & pin allocation, control-loop design, firmware structure, FreeRTOS decision.
- `ugv-motor-driver-encoders` — H-bridge driver selection, encoder PPR/calibration.
- `ugv-can-protocol` — CAN physical layer, node/message IDs, heartbeat/timeout.
- `ugv-raspberry-pi` — Pi Wi-Fi camera/network responsibilities and services.
- `ugv-camera-streaming` — camera hardware, capture/streaming pipeline, video usage.
- `ugv-operator-control` — control devices (Xbox/ELRS/Wi-Fi), PC control-station app.
- `ugv-power-electrical` — battery/BMS/charging, power rails, current/voltage/temperature sensing, wiring/EMC.
- `ugv-lighting-cooling` — lighting subsystem and behavior, cooling fan control.
- `ugv-telemetry-safety` — telemetry fields, logging schema, safety state machine, configuration management.
- `ugv-project-plan` — phased implementation sequence, test methodology, unresolved decisions, rejected/superseded approaches.
