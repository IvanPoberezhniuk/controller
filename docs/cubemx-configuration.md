# CubeMX Configuration Reference — STM32G431CBT6 UGV Motor Node

Every parameter set in `firmware/stm32-common/UGV_MotorNode.ioc` for this board, and why. Written
as a learning reference after building this project's Milestone 1
(one node, three motors, no CAN yet) — see `CLAUDE.md` for the wider project.

## MCU and project settings

| Setting | Value | Why |
|---|---|---|
| MCU | STM32G431CBTx | Confirmed hardware — LQFP48, 128KB flash, 32KB RAM, Cortex-M4F. Selected directly (not via a Nucleo board), since this is a custom PCB. |
| Toolchain/IDE | CMake | Portable, works with VS Code, not tied to a specific IDE's project format. |
| Application Structure | Advanced | Gives finer per-peripheral parameter control in the CubeMX GUI. **Not** physical file separation — checked the actual generated tree and every `MX_*_Init()` (`MX_TIM1_Init`, `MX_ADC2_Init`, etc.) still lands in `firmware/stm32-common/Core/Src/main.c`, not separate `tim.c`/`adc.c`/`usart.c` files. Hand-written application code stays outside generated `Core/`; platform-neutral CAN definitions live in `shared/can`. |

## Clock configuration

| Setting | Value | Why |
|---|---|---|
| SYSCLK source | HSI16 (internal 16 MHz RC) → PLL → 170 MHz | Raised from the initial no-PLL 16 MHz bring-up clock to the STM32G431's maximum-performance operating point once bring-up was working. PLL path: `/M=4` (16 MHz/4 = 4 MHz PLL input, within the 2.66–16 MHz valid range) → `×N=85` (4 MHz×85 = 340 MHz VCO, within the 96–344 MHz valid range) → `/R=2` (340/2 = 170 MHz SYSCLK). `PLLP`/`PLLQ` also come out at 170 MHz but are currently unused (no ADC-via-PLLP or FDCAN-via-PLLQ configured yet). |
| Voltage scaling | Range 1 Boost | 170 MHz is only reachable in Boost mode — normal Range 1 tops out at 150 MHz. |
| Flash latency | 4 wait states | Required at 170 MHz in Range 1 Boost per the reference manual's wait-state table (0 WS up to 34 MHz, ..., 4 WS up to 170 MHz). |
| AHB/APB1/APB2 prescalers | All ÷1 (undivided) | Keeps every peripheral clock at a single known value (170 MHz) — makes all the timer-period math below straightforward (still one number, just a different one than the original 16 MHz). |
| LSI | On | Required clock source for the independent watchdog (IWDG) — IWDG cannot run from HSI, and runs at ~32 kHz completely independent of SYSCLK/PLL either way. |

**Why this matters for timer math**: every PWM/timer period calculation in
this project (`Period = clock / target_frequency − 1`) depends on the
timer's actual input clock. **Raising SYSCLK via CubeMX does not
retroactively recompute dependent `Period`/`Prescaler` fields** — they're
stored as raw numbers in the `.ioc`, not formulas, so every clock-derived
register has to be re-audited by hand after a clock change. This bit the
project directly: after moving to 170 MHz, TIM1/TIM15's PWM period was
still the old 16 MHz-derived value (see below) and TIM6's control-loop
tick was silently running ~10.6x too fast until caught and fixed.

## TIM1 — motor0 + motor1 PWM

| Field | Value | Why |
|---|---|---|
| Channel1–4 | PWM Generation CH1/CH2/CH3/CH4 | TIM1 is an advanced timer with 4 independent compare channels — enough to cover two motors' RPWM+LPWM (2 channels each) from one timer. CH1/CH2 → motor0 RPWM/LPWM (PA8/PA9); CH3/CH4 → motor1 RPWM/LPWM (PA10/PA11). |
| Clock Source | Internal Clock | Without this, the timer has no clock at all and won't count — easy to miss because CubeMX will still generate PWM channel config code even with the timer unclocked. |
| Prescaler | 0 | No division — full 16 MHz timer clock. |
| Counter Period (ARR) | 8499 | `170,000,000 / 20,000 Hz − 1 = 8499` at the current 170 MHz timer clock. 20 kHz is a common brushed-DC PWM carrier frequency: high enough to be inaudible/efficient, comfortably within the BTS7960 driver's switching range. (Was `799` back when SYSCLK was 16 MHz with no PLL — recomputed after the clock change; the target frequency, 20 kHz, didn't change, only the register value needed to hit it. Bonus of the higher clock: duty-cycle resolution improved from 800 steps to 8500 steps.) |

## TIM15 — motor2 PWM

Same reasoning as TIM1 (Internal Clock, Prescaler 0, Period 8499 → 20 kHz).
TIM15 was used instead of the more "expected" TIM8 because TIM8's default
channel pins (`PC6`–`PC9`) aren't broken out on this 48-pin package — TIM15
is a smaller general-purpose timer that *is* available here, with the two
channels this motor needs.

## TIM2 / TIM3 / TIM4 — encoders (motor0 / motor1 / motor2)

| Field | Value | Why |
|---|---|---|
| Combined Channels | Encoder Mode | Puts the timer in hardware quadrature-decode mode — counts encoder edges in hardware with no CPU involvement, direction-aware. |
| Encoder Mode | TI1 and TI2 (not TI1-only or TI2-only) | Counts edges on **both** encoder channels for full x4 quadrature resolution. TI1-only (or TI2-only) counts both edges of just one channel — x2 decoding, half the resolution of TI1+TI2's x4. This was a real mistake caught mid-build (CubeMX defaulted to TI1-only) and corrected. |
| Counter Period | 32-bit max (TIM2) / 65535 (TIM3, TIM4) | TIM2 is a 32-bit counter on this MCU, TIM3/TIM4 are 16-bit — left at their respective max so the hardware counter never artificially wraps before the firmware reads it. (Firmware still handles rollover safely regardless, via `mm_encoder_delta()`.) |

TIM2/TIM3/TIM4 map to motor0/motor1/motor2 respectively because each motor
needs its own dedicated encoder-capable timer — this is also *why* the
project uses two separate STM32G431 boards (one per vehicle side) rather
than one MCU for all six motors: there are only three encoder-capable
general-purpose timers convenient for this on one chip.

## TIM6 — control-loop tick

| Field | Value | Why |
|---|---|---|
| Activated | Yes | TIM6 is a basic timer with no GPIO pins at all — purely an internal time base, used to pace the motor-control loop at a fixed rate rather than an arbitrary `HAL_Delay` loop. |
| NVIC global interrupt | *(intended, but never actually got enabled)* | The plan was interrupt-driven; in practice the interrupt was never turned on in the `.ioc`, so the firmware polls TIM6's update flag from the main loop instead (`firmware/stm32-common/Platform/Src/timebase.c`). Functionally fine for now — still timer-paced, just not interrupt-driven — but worth revisiting later for tighter timing jitter. |
| Period / Prescaler shown in `.ioc` | Left at CubeMX's default (65535) | The firmware overrides **both** at runtime in `firmware/stm32-common/Platform/Src/timebase.c` — `timebase_init()` explicitly calls `__HAL_TIM_SET_PRESCALER()` and `__HAL_TIM_SET_AUTORELOAD()` for a 500 Hz tick, rather than depending on getting both GUI fields exactly right. Keeps the control-loop rate a single documented constant in code instead of split between the `.ioc` and firmware. Current values (at the 170 MHz clock): `PSC=169` → `170,000,000/170 = 1,000,000` Hz counter clock, `ARR=1999` → `1,000,000/500 Hz − 1 = 1999`. TIM6 is a 16-bit basic timer (ARR max 65535), so — unlike TIM1/TIM15 above — a real prescaler is now required: `170,000,000/500 = 340,000` no longer fits in ARR alone with `PSC=0`. **This was caught as a real bug**: when SYSCLK moved from 16 MHz to 170 MHz, the old values (`PSC=0`, `ARR=31999`, correct for `16,000,000/32,000=500 Hz`) were left unchanged, silently producing a real tick rate of `170,000,000/32,000 = 5312.5 Hz` — ~10.6x too fast — which then fed a stale `dt_s=1/500` into every PID/ramp calculation in the control loop. Cheap way to sanity-check this in hardware without a scope: `app_main.c` prints a telemetry line every `TIMEBASE_CONTROL_LOOP_HZ/4` ticks (intended ~4 Hz) — at the bug's 5312.5 Hz it printed at ~42.5 Hz instead, visibly wrong on the console. |

## USART2 — debug console

| Field | Value | Why |
|---|---|---|
| Mode | Asynchronous | Simple point-to-point serial, no hardware flow control needed for a debug console. |
| Baud rate | 115200 | Standard, fast-enough default for a text console; not a bandwidth-critical link. |
| Word length / parity / stop bits | 8 / None / 1 | Standard "8N1" — the near-universal default for serial consoles. |

USART2 stands in for the eventual CAN command/telemetry link during
bring-up (no FDCAN configured yet — that's a later milestone once the
external CAN transceiver hardware is selected).

## IWDG — independent watchdog

| Field | Value | Why |
|---|---|---|
| Activated | Yes | Firmware-lockup protection — required from day one per this project's safety principles (never let a hung control loop keep driving motors). |
| Prescaler | ÷4 | Divides the ~32 kHz LSI clock down to ~8 kHz for the watchdog counter. |
| Reload / Window | 4095 / 4095 | Reload=4095 gives roughly a 512 ms timeout (`4096 / 8000 Hz`). Window=4095 (== Reload) disables window-mode early-refresh protection — simplest safe starting point; window mode can be added later if premature-refresh bugs become a concern. |

## Legacy generated state: ADC2 direct current-sense channels

The following section documents what is still present in `UGV_MotorNode.ioc`,
not the final multiplexer architecture. Do not enable
`UGV_MUX_GPIO_CONFIGURED` while ADC2 is configured as this five-rank scan.
The final manual CubeMX edit will replace these pins with one `MUX_SIG` ADC
input plus four S0-S3 GPIO outputs.

| Field | Value | Why |
|---|---|---|
| Channels enabled | IN3 (PA6, motor0 R_IS), IN4 (PA7, motor0 L_IS), IN12 (PB2, motor2 R_IS), IN13 (PA5, motor1 L_IS), IN17 (PA4, motor1 R_IS) | These five current-sense pins all happened to route through ADC2 on this package (checked live in CubeMX rather than assumed). |
| Single-ended (not Differential) | Single-ended | Each current-sense pin is read as a ground-referenced voltage on this MCU input, not a differential pair. Caveat: this describes the ADC's input mode, not a guarantee about the BTS7960 module itself — the IS pin's actual sense network (resistor value, any filtering) is module-dependent and needs to be verified against the real board before trusting the ADC reading as a calibrated current value (`current_sense_scale_a_per_v` in `configuration.h` is still a placeholder for this reason). Differential mode was CubeMX's default for two of these channels (IN3, IN12) and had to be explicitly overridden to Single-ended — a real mistake caught and fixed mid-build. |
| Scan Conversion Mode | Enabled | Required to sample more than one channel per ADC — steps through all enabled channels in Rank order each time a conversion is triggered. |
| Number Of Conversion | 5 | Must match the channel count exactly, or the scan sequence doesn't cover everything. |
| Rank → Channel mapping | Rank1=CH3, Rank2=CH4, Rank3=CH12, Rank4=CH13, Rank5=CH17 | Each Rank needs its own explicit Channel assignment — CubeMX's generated code silently reused the *first* channel for every rank when this wasn't set per-rank, another real mistake caught by reading the generated `main.c` rather than trusting the GUI. |

Firmware side (`firmware/stm32-common/Application/Src/current_monitor.c`): both ADCs are
calibrated once at startup via `HAL_ADCEx_Calibration_Start()` before any
conversions are trusted, and all ranks are read by polling
(`HAL_ADC_Start` → `HAL_ADC_PollForConversion` → `HAL_ADC_GetValue`, once
per rank) rather than DMA — simple and sufficient at this sample rate,
though a DMA circular buffer would be the natural upgrade if the control
loop rate increases later.

**Clock note (after the 16 MHz → 170 MHz change):** `ClockPrescaler =
ADC_CLOCK_SYNC_PCLK_DIV4` means the ADC's synchronous clock is now
`170,000,000/4 = 42.5 MHz` (was 4 MHz before). This is believed to be
within the STM32G4's synchronous ADC clock spec but has **not** been
checked against the datasheet's ADC clock table yet — treat as
unconfirmed until verified. Separately, real sample time in seconds
shrinks proportionally at the same `ADC_SAMPLETIME_2CYCLES_5` setting —
relevant when `current_sense_scale_a_per_v` (still a placeholder) is
eventually calibrated against real hardware.

## Legacy generated state: ADC1 remaining direct channel

ADC1 becomes unnecessary for motor-current acquisition after the manual MUX
pin assignment is complete.

| Field | Value | Why |
|---|---|---|
| Channel | IN11 (PB12, motor2 L_IS) | The one current-sense pin that happened to only be reachable via ADC1, not ADC2, on this package — split across two ADC peripherals purely because of which physical pins map to which ADC instance in silicon, not by design choice. |
| Single-ended | Single-ended | Same reasoning as ADC2. |

## GPIO outputs — motor driver enables

| Pin | Label | Why this pin |
|---|---|---|
| PB0 | `MOTOR0_R_EN` | Free GPIO, avoided debug (PA13/PA14), oscillator (PC14/15), and SWO (PB3) pins. |
| PB1 | `MOTOR0_L_EN` | Same reasoning, adjacent pin for a clean layout. |
| PB8 | `MOTOR1_R_EN` | Free GPIO after TIM1/TIM2/USART2/ADC pins were claimed. |
| PB9 | `MOTOR1_L_EN` | Same. |
| PB10 | `MOTOR2_R_EN` (intended) | Free GPIO — but the CubeMX **User Label never actually got set** on this pin despite repeated attempts, so `main.h` has no macro for it. Firmware works around this directly (`firmware/stm32-common/Platform/Inc/board.h` defines `MOTOR2_R_EN_REAL_Pin`/`_GPIO_Port` pointing at `GPIOB`/`GPIO_PIN_10` explicitly). Worth fixing properly next time the `.ioc` is regenerated: click PB10, re-enter the User Label. |
| PB11 | `MOTOR2_L_EN` | Free GPIO. |

Each motor's R_EN/L_EN are separate GPIOs (not tied together in software)
rather than one shared enable pin, so a stalled/faulted motor can be
disabled independently without cutting power to the other two.

## Pins deliberately left alone

| Pin(s) | Why |
|---|---|
| PA13 / PA14 | SWDIO/SWCLK — the ST-Link debug/programming lines. Reassigning these (which happened accidentally once, via an errant I2C1 config, and was caught and reverted) would break the ability to flash/debug over SWD. |
| PB3 | SWO trace pin — left alone to keep debug tracing available, even though it's not currently used. |
| PC14 / PC15 | Reserved for the LSE 32.768 kHz crystal, in case an RTC is added later. |

## Mistakes made and caught during this build (for pattern-recognition next time)

1. **Clicking a pin's function on the diagram sets GPIO routing but doesn't reliably activate the peripheral's own Mode.** TIM1/TIM2/TIM3/TIM4/TIM15/USART2/IWDG all needed their Mode set directly from the peripheral's own Category panel (Timers/Connectivity/System Core), not just by picking a function from a pin's dropdown. Only confirmed correct by reading the generated `Core/Src/main.c` and checking for a real `MX_<Peripheral>_Init()` call.
2. **A pin's displayed text in the Pinout view can show its User Label instead of its actual configured function**, which looks identical to a real conflict. Always verify by clicking the pin directly and checking which dropdown option is actually highlighted/selected, not just the label text shown on the diagram.
3. **ADC "Differential" is the CubeMX default for some channel numbers** (IN3, IN12 on this chip) — easy to silently get a wrong reading if not explicitly switched to Single-ended, and differential mode "borrows" the next channel number as its reference input, blocking it from being used independently.
4. **Multi-channel ADC scan sequences need each Rank's Channel set individually** — leaving it unset after the first Rank causes every Rank to silently reuse the first channel.
5. **Encoder Mode has three options (TI1 / TI2 / TI1 and TI2)** that all "work" (compile, no CubeMX warning) but only "TI1 and TI2" gives full quadrature resolution — the other two silently halve it.
6. **Raising SYSCLK in CubeMX (16 MHz → 170 MHz via PLL) does not recompute any dependent `Period`/`Prescaler` field.** Those are stored as raw numbers in the `.ioc`, not formulas re-derived from the clock tree. TIM1/TIM15's PWM period (`799`, a 16 MHz-derived value) and TIM6's hand-rolled control-loop prescaler/ARR (also 16 MHz-derived, and hardcoded a second time in `firmware/stm32-common/Platform/Src/timebase.c` rather than in the `.ioc`) were both silently wrong after the clock change until audited and fixed by hand. Rule going forward: after any SYSCLK change, re-derive every timer/ADC value in this document from scratch rather than assuming CubeMX carried them forward correctly — and grep the firmware source for hardcoded clock-frequency literals (e.g. `16000000u`) in addition to checking the `.ioc`.

## Future: FDCAN clock (not configured yet)

`PLLQ` now outputs 170 MHz alongside `PLLR`/`PLLP`, and is available as an
FDCAN kernel-clock source once the CAN milestone starts. No FDCAN bit-rate
math has been done yet — when it is, derive it from whichever clock source
gets selected at that time (likely `PLLQ` or `PCLK1`, both 170 MHz here),
not from the original 16 MHz assumption this document was first written
against.
