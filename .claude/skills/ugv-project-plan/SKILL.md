---
name: ugv-project-plan
description: TRIGGER when discussing the UGV project's implementation phases/sequence, test methodology, or when a hardware/protocol choice is about to be presented as final — always cross-check against the unresolved-decisions and rejected/superseded-approaches registers here first.
---

# UGV implementation plan, test methodology, and decision registers

## Implementation sequence

### Phase 1: requirements and electrical validation
Document exact motor labels/gearbox ratio; measure one motor (no-load
current, startup current, controlled stall current, current under
representative wheel load, encoder counts per output revolution); identify
encoder voltage/output type; select and test one motor driver; verify
battery/BMS configuration; establish safe bench power and fuse limits.
**Deliverable:** validated single-motor electrical report.

### Phase 2: one-motor STM32 prototype
One hardware encoder, one PWM output, direction/enable, fixed speed target,
measured RPM, basic PI controller, serial debug, command timeout, watchdog,
safe reset state. **Deliverable:** one motor follows positive/negative RPM
targets safely.

### Phase 3: three-motor node
Three hardware encoders, three drivers, three independent control loops,
motor-state array, current inputs, fault manager, acceleration limiting,
synchronized update loop. **Deliverable:** one complete side operates on a
test stand.

### Phase 4: CAN
FDCAN init, external transceiver, heartbeat, motion command, fast telemetry,
fault frames, sequence counter, timeout, bus-off recovery strategy.
**Deliverable:** PC or second MCU can command three motors over CAN.

### Phase 5: second motor node
Same firmware, another node ID. Validate left/right symmetry, motor/encoder
inversion, simultaneous six-motor operation, CAN bus loading, power
transients, thermal behavior. **Deliverable:** six-wheel bench test.

### Phase 6: ESP32 command gateway and Raspberry Pi Wi-Fi integration
XR4 CRSF parser, ESP32 MANUAL/AUTO control-source manager, TWAI final-command
publisher, Pi-to-ESP32 Wi-Fi AUTO requests, command/network heartbeats,
telemetry relay, SQLite logging, service supervision, configuration API.
**Deliverable:** ESP32 commands both STM32 nodes; Pi streams camera video,
requests future AUTO motion over Wi-Fi, and records relayed telemetry without
joining CAN.

### Phase 7: manual driving
Xbox input, PC application, throttle/steering mixing, arming, emergency
stop, 2WD/4WD/6WD modes, command loss behavior. **Deliverable:** low-speed
floor test with wheels unloaded, then loaded.

### Phase 8: camera
IMX708 capture, H.264 stream, MediaMTX RTSP, Qt video display, network-loss
behavior, CPU/temperature impact. **Deliverable:** stable video during full
motor operation.

### Phase 9: full power and thermal test
All-six startup current, straight driving current, turning current,
blocked-wheel current, 2WD/4WD/6WD consumption, Pi rail stability, driver
temperatures, battery sag, CAN error rate. **Deliverable:** final
fuse, cable, driver, cooling, and software-limit values.

### Phase 10: auxiliary systems
Lights, fan control, HGLRC M100-5883 GPS/compass, QMI8658A IMU, additional
telemetry, ELRS, future autonomy.

## Test methodology

For every subsystem provide: objective, test setup, instruments, procedure,
expected result, pass/fail criteria, recorded measurements, fault response,
next action.

Test levels:

```text
Unit test
-> bench subsystem test
-> one-side test
-> six-wheel unloaded test
-> low-voltage/current-limited test
-> full battery test
-> wheels-off-ground test
-> low-speed floor test
-> outdoor load test
```

**Do not make the first integrated test with:** full PWM, unrestricted
current, wheels on the ground, or no emergency stop.

## Unresolved decisions

Not yet final — do not represent as confirmed:

- final motor-driver model;
- final per-motor current limit;
- exact motor startup current from battery;
- exact encoder counts per output-shaft revolution;
- final current-sensor hardware;
- final fuse values after full-load measurement;
- exact 2WD and 4WD motor mapping;
- final lighting hardware;
- final SN65HVD230 protection/termination circuit;
- whether BMS CAN is available on the exact JK board;
- final ELRS command integration;
- final emergency-stop circuit;
- final suspension geometry;
- final enclosure dimensions;
- whether ROS 2 will be used in the initial operating version.

## Rejected or superseded approaches

Do not return to these without a documented reason:

- using a brushless ESC for the current brushed JGB37 motors;
- YS-60A RC ESC as the main motor-control solution;
- treating STM32F411 as if it contains native CAN;
- placing all six motors on one G431 solely to reduce board count;
- relying on Raspberry Pi Linux for real-time wheel PID and failsafe;
- placing Raspberry Pi permanently on the CAN trunk instead of using Wi-Fi/IP;
- sending HD video through ELRS;
- using seller peak-current claims as design values;
- continuing to use the old unsupported approximately 80 A drivetrain
  estimate as confirmed fact;
- continuing the last motor command forever after communication loss;
- powering motors directly before reset-safe enable logic is established.
