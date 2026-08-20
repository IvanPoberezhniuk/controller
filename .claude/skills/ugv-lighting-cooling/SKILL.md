---
name: ugv-lighting-cooling
description: TRIGGER when discussing the UGV's lighting subsystem (headlights, brake/reverse/indicator lights, status LEDs) or cooling-fan control logic (thresholds, staged/proportional speed, fault detection).
---

# UGV lighting and cooling

## Lighting

Desired auxiliary subsystem — no final lights, voltage, current, or exact
functions are confirmed.

Possible functions: front headlights, rear position lights, brake lights,
reverse lights, left/right indicators, status LEDs, fault indicator, work
light, IR illumination for a future camera, programmable RGB status
lighting.

Preferred architecture: do not power high-current lights directly from an
STM32 pin; use logic-level MOSFET drivers; use flyback protection for
inductive lamps/relays; use a separate fused lighting rail; use CAN commands
for lighting state; define safe default behavior.

Suggested behavior:

```text
Normal drive:
- front light configurable;
- rear position light on when enabled.

Braking:
- brake lights increase brightness.

Reverse:
- reverse light enabled.

Fault:
- status light displays fault pattern.

Communication loss:
- motors stop;
- hazard or fault light may activate.
```

## Cooling

A 120 mm fan is planned for the electronics compartment (see `ugv-mechanical`
for the physical mounting note). The cooling controller should consider Pi
temperature, motor-controller temperature, driver temperature, and battery
compartment temperature.

Initial approach: low-speed or off below a threshold; proportional or staged
control above threshold; full speed on sensor fault or severe temperature;
fan fault detection if a tachometer output exists.
