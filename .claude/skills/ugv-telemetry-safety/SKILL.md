---
name: ugv-telemetry-safety
description: TRIGGER when discussing the UGV's telemetry fields, Pi-side logging/database schema, the safety state machine (arming, emergency stop, reset behavior), or firmware configuration-value management.
---

# UGV telemetry, logging, safety, configuration

## Telemetry

Minimum fast telemetry: left/right target speed, six measured wheel speeds,
six PWM outputs, active drive mode, command sequence, command age, node
heartbeat, CAN status, system enabled state.

Minimum slow telemetry: pack voltage, pack current, battery SOC (when
trustworthy), driver temperatures when instrumented, Pi temperature, STM32
temperatures, total distance, per-wheel distance, fault counters, reset reason,
firmware versions, uptime, Wi-Fi status, ELRS RSSI/link quality when available.
Motor temperatures are reserved future telemetry and remain unavailable in
the initial build.

Telemetry must include units and validity flags.

## Logging (Raspberry Pi)

Suggested initial implementation: SQLite for structured local logs, rotated
text/JSONL logs for service diagnostics, optional binary log format for
high-rate telemetry later.

Suggested database tables:

```text
sessions
telemetry_samples
motor_samples
battery_samples
fault_events
operator_commands
can_frames_debug
system_events
configuration_changes
```

Use in-memory buffering, batch inserts, write-ahead logging, data-retention
limits, log rotation, clean shutdown handling.

## Safety architecture

### Hardware safety

Include: accessible physical emergency-stop circuit; driver enable that can
be removed independently of software; main fuse; branch fuses; safe output
states during STM32 reset; watchdog; battery undervoltage protection; thermal
protection; reverse-polarity protection; secure connectors. The emergency
stop should remove motor drive capability without necessarily killing the
Pi.

### Software safety states

```text
BOOT
DISABLED
ARMING
READY
ACTIVE
DEGRADED
FAULT
EMERGENCY_STOP
```

Motor outputs must be disabled in BOOT, DISABLED, FAULT, EMERGENCY_STOP.

### Arming conditions

Before arming: valid CAN communication; fresh command source; neutral
throttle; no emergency stop; acceptable battery voltage; no driver critical
faults; valid firmware state; encoder state plausible; watchdog operational.

### Reset behavior

After any reset: remain disabled; publish reset reason; require explicit
re-arm; **do not restore the previous motor command automatically.**

## Configuration management

Store configurable values with versioning, e.g.: wheel diameter, encoder
counts per revolution, motor/encoder direction inversion, PID coefficients,
PWM dead zone, maximum PWM, acceleration/deceleration rate, command timeout,
current limits, CAN node ID, drive-mode motor mapping. Temperature limits apply
only to a future revision with validated sensors.

Requirements: CRC or integrity check; defaults compiled into firmware;
version field; safe recovery after incompatible update; no unsafe motor
activation during configuration writes.
