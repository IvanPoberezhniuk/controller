---
name: ugv-stm32-firmware
description: TRIGGER when discussing STM32G431 hardware/peripherals, node or pin allocation, the motor-control loop (PID, ramping, stall detection), firmware directory structure, or the FreeRTOS-vs-bare-metal decision for the UGV's motor-control nodes.
---

# UGV STM32 firmware (motor-control nodes)

## STM32 node responsibilities

Each STM32 node performs: motor PWM generation, direction control, driver
enable control, encoder counting, wheel-speed calculation, per-wheel PID
control, acceleration/deceleration ramping, command-timeout detection,
emergency-stop response, watchdog handling, motor-temperature acquisition,
current-sensor acquisition (when added), local fault detection, CAN
communication, local telemetry publication, and deterministic safe-state
entry.

A motor-control STM32 must be able to stop its motors safely even if the
Raspberry Pi crashes, Linux freezes, CAN stops, the operator connection is
lost, or invalid commands are received. **The Pi must never perform
timing-critical wheel PID control** — Linux scheduling delays must not be
able to produce uncontrolled motor behavior.

## Confirmed MCU choice and allocation

2 x STM32G431CBT6: one MCU controls the three left-side motors, one controls
the three right-side motors. Both nodes run the *same* firmware with
configuration selecting the node role (left/right).

Relevant STM32G431CBT6 characteristics: LQFP48 package, 128 KB Flash, 32 KB
SRAM total (~16 KB SRAM1, ~6 KB SRAM2, ~10 KB CCM SRAM — do not assume the CCM
region is usable by every DMA peripheral; DMA-accessible buffers must be
placed in an appropriate SRAM region), industrial ambient rating
approximately -40 to +85 C, ~38 GPIOs, ~17 ADC channels, advanced
motor-control timers TIM1/TIM8, TIM2/TIM3/TIM4 support quadrature encoder
mode, one FDCAN controller (~1 KB message RAM, requires an external
transceiver), three USART + LPUART in this package, UART4 not available in
LQFP48.

**Why two boards, not one for all six motors:** TIM2/TIM3/TIM4 map cleanly to
three hardware quadrature-encoder interfaces per board; PWM/GPIO/ADC load
stays manageable; wiring is shorter mounting one board per side; faults are
isolated by side; software stays symmetric; CAN provides synchronization.
**Do not consolidate all six motors onto one G431** without a documented
reason and a validated alternate encoder strategy.

## Node/peripheral allocation

Example node IDs:

```text
0x10 — left motor controller
0x11 — right motor controller
0x20 — Raspberry Pi CAN gateway
0x30 — future battery-management gateway
0x40 — ESP32-S3 auxiliary/UI/lighting controller
```

Each motor node controls:

```text
Motor 0 — front
Motor 1 — center
Motor 2 — rear
```

Suggested peripheral allocation:

```text
TIM2 encoder mode — front motor
TIM3 encoder mode — center motor
TIM4 encoder mode — rear motor

TIM1 PWM channels — selected motor PWM outputs
TIM8 PWM channels — additional PWM outputs or synchronized motor control

FDCAN1 — vehicle CAN bus

ADC + DMA:
- motor temperatures;
- driver current measurements when added;
- controller supply voltage;
- optional board temperature.

Independent watchdog — firmware lockup protection
Hardware timer — control-loop scheduling
```

The exact pin map must be generated from STM32CubeMX and checked against the
physical board pinout before PCB/harness construction. Never assign pins
solely from generic package data without confirming the selected board
exposes them and doesn't use them for onboard hardware.

## Control-loop rates (initial, tunable)

```text
PWM carrier: approximately 18-25 kHz
Encoder sampling / velocity update: 500-1000 Hz
Wheel PID loop: 200-500 Hz
CAN command processing: event-driven
Fast status telemetry: 20-50 Hz
Detailed diagnostics: 1-10 Hz
Temperature telemetry: 1-5 Hz
```

The control loop should be timer-driven, not paced by arbitrary delays in the
main loop.

## Per-wheel state

```c
typedef struct {
    int32_t encoder_count;
    int32_t encoder_delta;

    float measured_rpm;
    float target_rpm;

    float pwm_command;
    float current_a;
    float temperature_c;
    bool current_valid;
    bool temperature_valid;

    float pid_integral;
    float previous_error;

    bool enabled;
    bool encoder_valid;
    bool overcurrent;
    bool overtemperature;
    bool driver_fault;
    bool stalled;
} MotorState;
```

## Command path

```text
CAN target command
    -> validate counter and timestamp
    -> apply mode restrictions
    -> apply command timeout logic
    -> acceleration limiter
    -> per-wheel target RPM
    -> PID/feed-forward controller
    -> PWM limiter
    -> driver output
```

## Control algorithm

Target wheel RPM + feed-forward PWM estimate + proportional/integral speed
correction (derivative only if measurement noise is controlled) + integrator
clamping + dead-zone compensation + acceleration/deceleration ramps + minimum
effective PWM calibration + direction-change interlock.

Direction change must decelerate to zero, disable/coast briefly, change
direction, then accelerate in the new direction. **Do not instantly reverse
motor polarity at significant speed.**

## Stall detection

Possible stall condition: target RPM above threshold, PWM or current above
threshold, measured RPM below threshold, condition persists for a configured
duration. Reaction: reduce command or disable the affected motor, publish
fault, optionally disable the full side, escalate to complete vehicle stop if
steering becomes unsafe. Thresholds must be based on measured data.

## Firmware structure

```text
firmware/
    stm32-common/
        Core/                 CubeMX-generated shell
        Drivers/              STM32 HAL/CMSIS
        Application/          shared motor/safety logic
        Platform/             board, watchdog, timebase
    stm32-left/node_config.h
    stm32-right/node_config.h
shared/can/                   wire contract, codec, DBC
Tests/stm32/                  host-side pure/state-machine tests
```

Separate generated STM32CubeMX code, board-specific code, application logic,
CAN protocol, and testable algorithms. **Do not place all logic directly in
`main.c`.** Use C or constrained embedded C++ consistently; avoid dynamic
allocation in timing-critical firmware after initialization.

## FreeRTOS decision

FreeRTOS may be used but is **not required** for the first motor-controller
firmware — a timer-driven bare-metal architecture is sufficient for three
encoders, three PID loops, CAN, ADC DMA, telemetry, watchdog, and the safety
state machine. Use FreeRTOS only when it provides a concrete benefit.

If selected, possible tasks:

```text
MotorControlTask — highest normal priority
SafetyTask
CanRxTask
TelemetryTask
SensorTask
DiagnosticsTask
```

Motor control should still be paced by a hardware timer or precise
notification either way.
