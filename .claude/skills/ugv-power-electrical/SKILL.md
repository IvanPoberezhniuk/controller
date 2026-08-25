---
name: ugv-power-electrical
description: TRIGGER when discussing the UGV's battery/BMS/charging, power rail distribution, Raspberry Pi power supply, current/voltage sensing, board/driver temperature monitoring, or wiring/EMC practices.
---

# UGV power and electrical

## Battery chemistry and configuration

Current concept: LiFePO4 chemistry, four 40150-format cells (EVE 40150S
mentioned in later planning), 4S1P initially (12.8 V nominal, 14.6 V fully
charged), 4S2P considered for increased capacity/current, earlier desired
capacity around 20 Ah.

One cell had a small dent — **treat mechanically damaged cells cautiously**;
do not assume that cell is safe without inspection and testing.

## BMS

Current smart BMS: JIKONG/JK, model JK-BD4A8S6P, supports 4S-8S,
approximately 60 A rating, Bluetooth, active balancing.

Previous issues: configuration write errors; SOC showing 100%; charger
supplying around 13.6 V; difficulty setting approximately 3.40 V per cell;
uncertainty about whether the exact board version exposes CAN. **Do not
assume the BMS has usable CAN merely because some variants support it** —
check the physical board for CANH/CANL, a CAN-marked connector, exact PCB
revision, and protocol documentation.

## Charging

```text
USB-C PD 65 W source
    -> USB-C PD trigger or CH224-based sink
    -> 20 V
    -> XL4016 CC/CV buck converter
    -> 14.6 V, approximately 4 A
    -> BMS and battery
```

Requirements: 14.6 V maximum for 4S LiFePO4; current limit appropriate for
cells/BMS; verify XL4016 thermal performance; independently verify output
voltage before battery connection.

## Main wiring

Previously considered: 8 AWG main battery wiring, XT60-class main connector,
main fuse around 50-60 A, 60 A BMS.

Given the newer 3.6 A motor measurement (see `ugv-drivetrain`), the final
fuse may be lower, but must account for six-motor startup, Pi/auxiliaries,
temporary wheel stalls, expected driver behavior, cable protection, BMS trip
characteristics. **Do not reduce the main fuse until full-pack current
measurements are available.**

## Power rails

Use separate fused branches:

```text
Battery raw rail: approximately 10-14.6 V
    -> six motor drivers

Battery raw rail
    -> protected DC/DC converter for Raspberry Pi 5

Battery raw rail
    -> regulated MCU and sensor rail

Battery raw rail
    -> lighting rail

Battery raw rail
    -> fan and auxiliary rail
```

Avoid powering logic through motor-driver regulator outputs.

## Raspberry Pi power

The Pi needs a stable regulated supply independent of motor transients:
regulated 5 V rail sized for Pi 5 load, sufficient continuous current
(generally 5 A class depending on peripherals), short low-resistance wiring,
local bulk capacitance, undervoltage monitoring, filtering from motor noise,
correct grounding, no reliance on a weak generic buck converter without load
testing.

Test under: camera streaming, Wi-Fi active, CPU load, CAN interface, USB
devices, lighting transitions, all motors starting. **The Pi must not reboot
during motor startup or braking.**

## Current and voltage sensing

No final current-sensor hardware confirmed. Required measurements: total
battery voltage, total battery current, preferably individual driver/motor
current, Pi supply voltage, motor-controller supply voltage. Used for: energy
consumption, stall detection, traction diagnostics, driver selection, fuse
validation, 2WD/4WD/6WD comparison, estimated battery runtime.

## Temperature monitoring

Driver, STM32-board, battery-area, and Pi-area thermal protection/monitoring
remain requirements. Validate each interface before allocating pins or wiring,
and use filtering and plausibility checks for any long analog lead near motor
wiring.

## Wiring and EMC

Motor power and encoder signals share the factory cable bundle. Requirements:
twist motor power pairs where possible; twisted pairs for encoder A/B or
signal/ground; avoid long unshielded high-impedance analog lines; add input
filtering; use Schmitt-trigger-capable inputs where possible; separate
power/signal connectors on the controller; avoid ground loops; route CAN
separately from motor leads where practical; proper strain relief; keep
encoder cables away from PWM switching nodes on the PCB.

Add firmware diagnostics for: impossible encoder speed jumps, encoder
movement while motor disabled, no encoder movement under sustained command,
invalid current readings, CAN error counts.
