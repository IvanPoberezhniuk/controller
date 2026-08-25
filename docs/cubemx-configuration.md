# CubeMX Configuration Reference — STM32G431CBT6 UGV Motor Node

This is the verified CubeMX reference for the shared STM32G431CBT6 motor-node
project. Left and Right use the same `.ioc`; role-specific behavior is selected
by the CMake preset. The checked-in `.ioc` and generated `Core/` code already
contain the final CAN, PWM, common-enable, and six-input direct-ADC layout.

## Applied final pin configuration

The following settings are already applied in
`firmware/stm32-common/UGV_MotorNode.ioc`. Preserve them together during future
edits; a partial rollback creates pin conflicts or drives the center motor
incorrectly.

1. In TIM1 disable PWM Generation CH4 on PA11. Keep CH1/CH2/CH3 on
   PA8/PA9/PA10.
2. Enable TIM16, internal clock, PWM Generation CH1 on PB8. Set Prescaler `0`,
   Counter Period `8499`, Pulse `0`, PWM mode 1, active-high. Label it
   `MOTOR1_LPWM`.
3. Use one common-enable output per motor: PB0=`MOTOR0_EN`, PB9=`MOTOR1_EN`,
   PB10=`MOTOR2_EN`. Configure them push-pull, no pull, low speed, initial low.
   Each output fans out to both R_EN and L_EN inputs of its own driver. Remove
   the old PB1/PB11 enable outputs; PB8 changes to TIM16 in step 2.
4. Keep all six direct current-sense inputs. ADC2 must be a five-rank,
   single-ended scan: rank1 PA6/IN3 front R_IS, rank2 PA7/IN4 front L_IS,
   rank3 PB2/IN12 rear R_IS, rank4 PA5/IN13 center L_IS, rank5 PA4/IN17 center
   R_IS. ADC1 remains one channel: PB12/IN11 rear L_IS. Use software trigger,
   non-continuous mode, EOC after each conversion, and start with 92.5-cycle
   sampling time on every rank.
5. Select SYSCLK as the ADC12 kernel clock and asynchronous divide-by-8 for
   both ADCs: `170 MHz / 8 = 21.25 MHz` ADC clock.
6. Enable FDCAN1 in Classic CAN normal mode on PA11=`FDCAN1_RX` and
   PA12=`FDCAN1_TX`, using the exact timing in the FDCAN section below.
7. Set the standard-filter count to `1` and TX mode to FIFO operation. The G4
   HAL exposes a fixed message-RAM layout rather than CubeMX element-count
   fields; the application update service uses standard filter index 0 and RX
   FIFO0.
8. Generate code with **Keep User Code when re-generating** enabled. Confirm
   `MX_TIM16_Init()` and `MX_FDCAN1_Init()` are called before
   `app_main_init()`.
9. Build both OTA applications and bootloaders with
   `tools/build-update-images.ps1`. The final pinout is unconditional.
10. After the R_IS/L_IS conditioning is installed and calibrated, replace the
    placeholder `current_sense_scale_a_per_v` and current thresholds, then set
    `UGV_CURRENT_SENSE_CALIBRATED` to `1`. Do not set it merely because ADC
    conversion succeeds; the value must represent verified amperes.

The custom bootloader does not use this CubeMX initialization. It configures
HSI16, PA11/PA12, FDCAN, flash, and safe output levels independently, so it
remains a recovery path even if an application image is broken.

## MCU and project settings

| Setting | Value | Why |
|---|---|---|
| MCU | STM32G431CBTx | Confirmed hardware — LQFP48, 128KB flash, 32KB RAM, Cortex-M4F. Selected directly (not via a Nucleo board), since this is a custom PCB. |
| Toolchain/IDE | CMake | Portable, works with VS Code, not tied to a specific IDE's project format. |
| Application Structure | Advanced | Gives finer per-peripheral parameter control in the CubeMX GUI. **Not** physical file separation — checked the actual generated tree and every `MX_*_Init()` (`MX_TIM1_Init`, `MX_ADC2_Init`, etc.) still lands in `firmware/stm32-common/Core/Src/main.c`, not separate `tim.c`/`adc.c`/`usart.c` files. Hand-written application code stays outside generated `Core/`; platform-neutral CAN definitions live in `shared/can`. |

## Clock configuration

| Setting | Value | Why |
|---|---|---|
| SYSCLK source | HSI16 (internal 16 MHz RC) → PLL → 170 MHz | Raised from the initial no-PLL 16 MHz bring-up clock to the STM32G431's maximum-performance operating point once bring-up was working. PLL path: `/M=4` (16 MHz/4 = 4 MHz PLL input, within the 2.66–16 MHz valid range) → `×N=85` (4 MHz×85 = 340 MHz VCO, within the 96–344 MHz valid range) → `/R=2` (340/2 = 170 MHz SYSCLK). `PLLP`/`PLLQ` also come out at 170 MHz but remain unused; FDCAN uses PCLK1. |
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

## TIM1 — front PWM plus center RPWM

| Field | Value | Why |
|---|---|---|
| Channels 1–3 | PWM Generation CH1/CH2/CH3 | CH1/CH2 drive front RPWM/LPWM on PA8/PA9. CH3 drives center RPWM on PA10. CH4 must be disabled so PA11 can be FDCAN1_RX. |
| Clock Source | Internal Clock | Without this, the timer has no clock at all and won't count — easy to miss because CubeMX will still generate PWM channel config code even with the timer unclocked. |
| Prescaler | 0 | No division — full 170 MHz timer clock. |
| Counter Period (ARR) | 8499 | `170,000,000 / 20,000 Hz − 1 = 8499` at the current 170 MHz timer clock. 20 kHz is a common brushed-DC PWM carrier frequency: high enough to be inaudible/efficient, comfortably within the BTS7960 driver's switching range. (Was `799` back when SYSCLK was 16 MHz with no PLL — recomputed after the clock change; the target frequency, 20 kHz, didn't change, only the register value needed to hit it. Bonus of the higher clock: duty-cycle resolution improved from 800 steps to 8500 steps.) |

## TIM16 — center LPWM

| Field | Value | Why |
|---|---|---|
| Channel 1 | PWM Generation CH1 on PB8 | Replaces TIM1_CH4/PA11 and frees PA11 for FDCAN RX. PB8 also serves as BOOT0 during one-time provisioning. |
| Clock source | Internal clock | Uses the same 170 MHz timer clock as TIM1. |
| Prescaler / Period / Pulse | `0` / `8499` / `0` | 20 kHz, initially zero duty. Application code always uses `htim16` for center LPWM. |

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
| Period / Prescaler shown in `.ioc` | Left at CubeMX's default (65535) | `timebase_init()` derives both values at runtime from the live APB1 timer clock and the 500 Hz requirement. This avoids duplicating the current 170 MHz SYSCLK in application code. TIM6 remains a 16-bit timer, so the calculation selects the smallest prescaler that keeps ARR within 16 bits. |

## USART2 — debug console

| Field | Value | Why |
|---|---|---|
| Mode | Asynchronous | Simple point-to-point serial, no hardware flow control needed for a debug console. |
| Baud rate | 115200 | Standard, fast-enough default for a text console; not a bandwidth-critical link. |
| Word length / parity / stop bits | 8 / None / 1 | Standard "8N1" — the near-universal default for serial consoles. |

USART2 remains the bench console and is also the supported one-time path into
the STM32 factory ROM bootloader. Application commands and OTA updates use
FDCAN. The application console is 8N1; the ROM
bootloader protocol uses even parity, which STM32CubeProgrammer selects for the
factory-provisioning session.

## IWDG — independent watchdog

| Field | Value | Why |
|---|---|---|
| Activated | Yes | Firmware-lockup protection — required from day one per this project's safety principles (never let a hung control loop keep driving motors). |
| Prescaler | ÷4 | Divides the ~32 kHz LSI clock down to ~8 kHz for the watchdog counter. |
| Reload / Window | 4095 / 4095 | Reload=4095 gives roughly a 512 ms timeout (`4096 / 8000 Hz`). Window=4095 (== Reload) disables window-mode early-refresh protection — simplest safe starting point; window mode can be added later if premature-refresh bugs become a concern. |

## Direct current sensing: ADC2 scan

This five-rank scan is the final architecture and is already represented in
`UGV_MotorNode.ioc`. Preserve it while making the FDCAN/TIM16 changes.

| Field | Value | Why |
|---|---|---|
| Channels enabled | IN3 (PA6, motor0 R_IS), IN4 (PA7, motor0 L_IS), IN12 (PB2, motor2 R_IS), IN13 (PA5, motor1 L_IS), IN17 (PA4, motor1 R_IS) | These five current-sense pins all happened to route through ADC2 on this package (checked live in CubeMX rather than assumed). |
| Single-ended (not Differential) | Single-ended | Each current-sense pin is read as a ground-referenced voltage on this MCU input, not a differential pair. Caveat: this describes the ADC's input mode, not a guarantee about the BTS7960 module itself — the IS pin's actual sense network (resistor value, any filtering) is module-dependent and needs to be verified against the real board before trusting the ADC reading as a calibrated current value (`current_sense_scale_a_per_v` in `configuration.h` is still a placeholder for this reason). Differential mode was CubeMX's default for two of these channels (IN3, IN12) and had to be explicitly overridden to Single-ended — a real mistake caught and fixed mid-build. |
| Scan Conversion Mode | Enabled | Required to sample more than one channel per ADC — steps through all enabled channels in Rank order each time a conversion is triggered. |
| Number Of Conversion | 5 | Must match the channel count exactly, or the scan sequence doesn't cover everything. |
| Rank → Channel mapping | Rank1=CH3, Rank2=CH4, Rank3=CH12, Rank4=CH13, Rank5=CH17 | Each Rank needs its own explicit Channel assignment — CubeMX's generated code silently reused the *first* channel for every rank when this wasn't set per-rank, another real mistake caught by reading the generated `main.c` rather than trusting the GUI. |
| Sampling time | 92.5 cycles initially | Provides acquisition margin for the final R_IS/L_IS conditioning network; reduce only after bench validation. This value is checked into the `.ioc` and generated initialization. |

Firmware side (`firmware/stm32-common/Application/Src/current_monitor.c`): both ADCs are
calibrated once at startup via `HAL_ADCEx_Calibration_Start()` before any
conversions are trusted, and all ranks are read by polling
(`HAL_ADC_Start` → `HAL_ADC_PollForConversion` → `HAL_ADC_GetValue`, once
per rank) rather than DMA — simple and sufficient at this sample rate,
though a DMA circular buffer would be the natural upgrade if the control
loop rate increases later.

**Clock note:** the `.ioc` selects SYSCLK for ADC12 and both ADCs use
`ADC_CLOCK_ASYNC_DIV8`, producing `170/8 = 21.25 MHz`. The generated
`HAL_ADC_MspInit()` applies `RCC_ADC12CLKSOURCE_SYSCLK` before enabling ADC1 or
ADC2. Validate ADC timing and the final R_IS/L_IS scale against the assembled
conditioning network.

## Direct current sensing: ADC1 rear L_IS

ADC1 remains enabled for the sixth direct current-sense signal.

| Field | Value | Why |
|---|---|---|
| Channel | IN11 (PB12, motor2 L_IS) | The one current-sense pin that happened to only be reachable via ADC1, not ADC2, on this package — split across two ADC peripherals purely because of which physical pins map to which ADC instance in silicon, not by design choice. |
| Single-ended | Single-ended | Same reasoning as ADC2. |
| Sampling time | 92.5 cycles initially | Match the ADC2 current-sense ranks and validate on hardware. |

## GPIO outputs — common motor-driver enables

| Pin | Label | Why this pin |
|---|---|---|
| PB0 | `MOTOR0_EN` | Drives front-driver R_EN and L_EN together. |
| PB9 | `MOTOR1_EN` | Drives center-driver R_EN and L_EN together; PB8 is center LPWM. |
| PB10 | `MOTOR2_EN` | Drives rear-driver R_EN and L_EN together. |

PB1, PB11, and PB13 are free GPIO reserve. Each motor still has an
independent enable output, so a faulted motor can be disabled without cutting
power to the other two. Fit a 10 kohm pull-down on every common-enable net.

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
6. **Raising SYSCLK in CubeMX does not recompute every raw peripheral field.** Re-check ADC and PWM settings after a clock-tree change. Application PWM writes read the live timer ARR, and TIM6 derives its runtime prescaler/ARR from APB1, so those consumers no longer duplicate the 170 MHz literal.

## FDCAN1 — motor network and firmware update

| Field | Value | Why |
|---|---|---|
| Pins | PA11 RX, PA12 TX | Keeps CAN away from PB8/BOOT0 and matches the custom bootloader. |
| Kernel clock | PCLK1, 170 MHz | Already available with the current undivided clock tree. |
| Frame format / mode | Classic CAN / Normal | Compatible with ESP32 TWAI and a temporary SocketCAN service adapter. Raspberry Pi is not on the bus. |
| Auto retransmission | Enabled | Hardware retries arbitration/errors; higher-level OTA sequence ACK still handles lost windows. |
| Nominal prescaler | `10` | Produces a 17 MHz time-quantum clock. |
| Nominal time segment 1 | `29` | With SyncSeg=1 and TSEG2=4, total is 34 time quanta. |
| Nominal time segment 2 | `4` | Sample point is `(1 + 29) / 34 = 88.2%`. |
| Nominal SJW | `4` | Within TSEG2 and tolerant of oscillator/edge error. |
| Result | 500000 bit/s | `170 MHz / (10 * 34) = 500 kbit/s`. |
| Standard filters | `1` | `fw_update_service` installs the exact command-ID filter at index 0. |
| Message RAM / TX mode | STM32G4 fixed layout / TX FIFO operation | The G4 HAL does not expose the generic CubeMX FIFO element-count fields. All current protocol frames are Classic CAN DLC 8 or smaller and arrive through RX FIFO0. |

The running application's current FDCAN service only handles the safe request
to enter the bootloader. It rejects other incoming identifiers until the full
operational command/telemetry transport is added. Do not treat this interim
filter configuration as completed vehicle CAN control.
