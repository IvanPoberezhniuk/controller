---
name: ugv-motor-driver-encoders
description: TRIGGER when discussing H-bridge / motor-driver selection for the UGV's brushed DC motors, or the wheel encoder subsystem (PPR, calibration, quadrature counting, wheel-speed conversion).
---

# UGV motor driver and encoder subsystem

## Motor driver selection

The JGB37 motors are brushed DC — they require a bidirectional H-bridge
driver (not a BLDC ESC), PWM control, direction control, adequate transient
current capacity, suitable thermal design, and protection from inductive
transients. **Do not recommend a three-phase BLDC ESC for these motors.**

Previously considered options: BTS7960/IBT-2, VNH5019, Cytron brushed-motor
drivers, YS-60A RC ESC (dropped), other smaller H-bridges after the 3.6 A
stall-current measurement (see `ugv-drivetrain`). BTS7960 was sized for
earlier, much higher current estimates and may now be unnecessarily large.
**No final replacement driver has yet been confirmed.**

Select a driver based on validated measurements, not seller-advertised peak
current. Required properties: one independent full H-bridge per motor;
continuous current comfortably above real loaded current; transient current
above measured startup/stall current; operation at approximately 12-14.6 V;
logic compatibility with STM32G431; PWM frequency suitable for quiet/efficient
operation; direction or dual-PWM control; enable input or controllable safe
disable; thermal shutdown; overcurrent/short-circuit protection; undervoltage
protection; preferably current-sense output; low on-resistance; documented
truth table; predictable behavior when MCU pins float during reset.

Design target pending better measurement: continuous capability at least
approximately 5-6 A per motor channel, transient capability above
approximately 10 A per channel desirable — final value based on
battery-powered startup and mechanical stall tests. **Do not select a driver
solely because its listing says "43 A", "60 A", or another unrealistic
marketing value.**

### Control interface

Preferred per motor: `PWM`, `DIR`, `ENABLE`/`FAULT`, `CURRENT_SENSE` (when
available). Dual-PWM drivers also acceptable: `PWM_A`, `PWM_B`, `ENABLE`,
`FAULT`. Firmware must guarantee invalid high-side/low-side combinations
cannot produce shoot-through. During reset: outputs must default to motor
disabled, hardware pull-down/pull-up resistors must establish the safe state,
driver enable must stay inactive until firmware init finishes.

### Braking

Support configurable coast, active brake, controlled deceleration. **Do not
abruptly apply active braking at high wheel speed** until mechanical/current
testing confirms it's safe for gears, motor drivers, battery, BMS, wheels and
hubs.

## Encoder subsystem

Six motor encoders, advertised resolution approximately 11 PPR at the motor
shaft. **Do not assume** whether "11 PPR" means 11 cycles/rev, 11 pulses on
one channel, 22 edges on one channel, or 44 counts with x4 quadrature —
determine this experimentally.

Calibration procedure:
1. rotate the output shaft a known number of complete gearbox revolutions;
2. count raw timer transitions;
3. calculate counts per output-shaft revolution;
4. repeat in both directions;
5. verify whether gearbox backlash affects low-speed readings;
6. store the resulting constant in configuration.

Each G431 uses hardware encoder mode for its three motors. Required features:
signed position count, rollover-safe delta calculation, speed calculation
over a fixed interval, low-speed estimation, direction detection,
invalid-transition detection where possible, zero-speed timeout, per-wheel
calibration, diagnostic detection for a disconnected/frozen encoder.

Wheel-speed conversion:

```text
delta_counts
    -> motor/output revolutions
    -> wheel revolutions per second
    -> wheel RPM
    -> linear speed using wheel circumference
```

**Do not use the nominal gearbox RPM as feedback — use encoder-derived wheel
speed.**
