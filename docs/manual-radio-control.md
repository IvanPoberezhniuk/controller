# Manual radio control

```text
connectionApp -> Nomad TX -> RF -> XR4 -> CRSF -> ESP32
                                           |-- UART Left --> STM32 Left
                                           `-- UART Right -> STM32 Right
```

Raspberry Pi, Wi-Fi, CAN transceivers, and extra runtime equipment are not
required to drive. ESP32 and both STM32 boards must contain the matching
dual-UART firmware.

## Connections used by firmware

| Link | TX endpoint | RX endpoint | Baud |
| --- | --- | --- | ---: |
| XR4 → ESP32 | XR4 TX | ESP GPIO21 | 420000 |
| ESP32 → XR4 telemetry | ESP GPIO38 | XR4 RX | 420000 |
| Left command | ESP GPIO39 | Left STM PA3 | 115200 |
| Left telemetry | Left STM PA2 | ESP GPIO40 | 115200 |
| Right command | ESP GPIO41 | Right STM PA3 | 115200 |
| Right telemetry | Right STM PA2 | ESP GPIO42 | 115200 |

Connect a common logic GND. UART TX always connects to the other endpoint's
RX. Brown identifies ESP→STM command TX and purple identifies STM→ESP
telemetry TX. SN65HVD230 modules and CAN-H/CAN-L are not connected.

## connectionApp channel contract

| CRSF channel | Function | Behavior |
| ---: | --- | --- |
| CH1 | Steering | left/right skid-steer mix |
| CH2 | Throttle | forward/reverse |
| CH3 | Drive mode | low=rear 2WD, center=middle+rear 4WD, high=6WD |
| CH4 | Lights | reserved |
| CH5 | ARM | low=disarm; neutral low-to-high edge=arm |
| CH6 | ESTOP | high latches emergency stop |
| CH7 | CLEAR_FAULT | high clears a latched STM32 FAULT; never arms by itself |

The current mixer is:

```text
left  = throttle - steering * 0.5
right = throttle + steering * 0.5
```

The initial radio limit is 200 RPM. Each side's packet still contains three
independent RPM values and a per-wheel enable mask, so future control code can
command each wheel separately. STM32 telemetry returns the six measured RPMs;
ESP32 alternates left/right RPM groups back to XR4 as CRSF telemetry.

Once per second ESP32 also sends a private CRSF passthrough frame (`type 0x80`,
payload signature `UGV`, version 1). It reports RF/ARM/ESTOP state, normalized
controls, CRSF counters, and for each motor UART: command TX/fail counts,
telemetry RX count and age, parser CRC/format errors, STM32 safety state,
fault mask, and validity mask. ConnectionApp decodes these frames into its
Logs panel and `ugv-control.log`.

## Safe first start

1. Raise all wheels and keep a physical power disconnect accessible.
2. Keep ARM low, ESTOP low, throttle and steering centered.
3. Power the 3.3 V logic rail, the 5 V XR4 rail, and both STM32 nodes.
4. Start connectionApp and confirm Nomad and XR4 have a solid RF link.
5. Verify ESP GPIO39 and GPIO41 produce periodic UART traffic before applying
   motor power if a logic analyzer is available.
6. Install motor fuses, switch ARM low then high while controls are centered,
   and apply only a tiny throttle command.
7. Confirm every wheel direction and immediately disarm on any mismatch.

ARM is edge-qualified. Starting with CH5 already high cannot arm. After ESTOP
or link loss, return ARM low, center controls, keep ESTOP low, then raise ARM
again.

## Failure behavior

- Bad CRC, wrong node role, malformed data, or out-of-range RPM is rejected.
- A swapped Left/Right harness is rejected by the STM32 role check.
- No valid command for 300 ms latches that node into `FAULT` (motors forced
  off) if it was armed; reconnecting/re-arming from connectionApp does not
  clear it. Raise CH7 (CLEAR_FAULT) to return the node to `DISABLED`, then
  raise ARM again as usual. This is the expected result of restarting
  connectionApp while armed, not a hardware failure.
- CRSF loss disarms commands at ESP32 after 100 ms.
- Reset of any controller begins disabled; previous targets are not restored.
