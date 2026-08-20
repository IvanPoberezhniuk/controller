---
name: ugv-drivetrain
description: TRIGGER when discussing the UGV's motors, wheels/tires, motor current measurements, top-speed calculations, or 2WD/4WD/6WD drive-mode selection and skid-steer mixing — JGB37 motor specs, stall current, gear ratio, wheel diameter, throttle/steering mixing math.
---

# UGV drivetrain: motors, wheels, drive modes

## Confirmed motors

Six JGB37-520-class brushed DC gearmotors with encoders.

- six motors total, one motor per wheel;
- nominal motor supply: 12 V;
- gearbox output speed: approximately 333 RPM;
- encoder advertised as approximately 11 pulses per revolution at the motor
  side (verify experimentally — see `ugv-motor-driver-encoders`).

A 133 RPM motor version was previously considered, but the 333 RPM reduction
version is confirmed for the current drivetrain.

## Wheels

- outer tire diameter: approximately 118 mm;
- tire width: approximately 45 mm;
- rim format: approximately 1.9 inch;
- printed wheel hub/adapter;
- motor shaft/hub interface previously discussed around 15 mm;
- rim depth approximately 45 mm.

At 333 RPM with a 118 mm tire: circumference 0.371 m, theoretical no-load
ground speed approximately 7.4 km/h. The earlier desired top speed was
approximately 10 km/h — **do not claim 10 km/h is achieved** without a change
in gearing, tire diameter, or motor speed.

## Measured current (bench test)

- supply voltage: approximately 14 V, supply current limit 10 A;
- no-load current previously observed around 0.1 A;
- motor physically stalled by hand: maximum observed current approximately
  3.6 A; motor remained energized and attempted to rotate.

This measurement is more relevant than earlier theoretical estimates. However:
the bench power supply may have influenced the result; wiring resistance,
driver resistance, supply response, gearbox friction, and motor condition may
affect it. **Repeat with a proper current sensor and representative battery
wiring before final driver/fuse sizing.**

For preliminary engineering, use:
- approximately 3.6 A measured peak per motor;
- up to approximately 21.6 A if all six simultaneously reach this condition;
- additional margin for startup, wheel impacts, transients, temperature,
  measurement uncertainty.

**Do not continue using the old, unsupported ~80 A total estimate as if
confirmed** (see rejected approaches in `ugv-project-plan`).

## Drive modes

### 2WD
Rear-left and rear-right motors active; other four inactive or free-running
depending on mechanical/electrical test results. Use for reduced energy
consumption, light-load travel, testing, emergency degraded mode.

### 4WD
Four selected motors active; the exact axle combination must be configurable,
not hard-coded. Likely default: front and rear axles active, center axle
inactive. Final selection must be validated experimentally.

### 6WD
All six motors active. A torque-limited mode was discussed: approximately
40-50% configured torque/current limit, potentially reduced speed (earlier
low-speed target around 50 RPM at the wheel). **This must not be implemented
as an arbitrary PWM cap** — prefer a configurable combination of wheel-speed
target, current limiting (when sensors available), acceleration ramp, max PWM
duty, thermal protection, and traction-related logic.

## Steering

Normal steering is skid steering:

```text
left_target  = throttle + steering
right_target = throttle - steering
```

Then clamp and normalize. Each wheel controller receives its side target,
potentially modified by per-wheel trim and traction logic.

Experimental control modes discussed (progressive per-side motor activation,
locking specific wheels during maneuvers, staged turning via Xbox trigger
position) are **experimental only** — do not make them the default
safety-critical steering strategy.
