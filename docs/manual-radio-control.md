# Manual radio control

The implemented manual path is:

```text
connectionApp -> ELRS TX module -> radio link -> XR4
              -> CRSF UART -> ESP32 -> 500 kbit/s CAN
              -> STM32 Left + STM32 Right -> six motors
```

Raspberry Pi, Wi-Fi, and a separate runtime controller are not required for
manual driving. The ESP32 and both STM32 boards must first contain the current
firmware images.

## CRSF and CAN wiring used by firmware

| Signal | Connection |
| --- | --- |
| XR4 TX | ESP32 GPIO21 (CRSF RX) |
| XR4 RX | ESP32 GPIO38 (CRSF TX) |
| SN65HVD230 RX | ESP32 GPIO18 (CAN RX, purple wire) |
| SN65HVD230 TX | ESP32 GPIO17 (CAN TX, brown wire) |

UART signal names cross because each endpoint's TX drives the other
endpoint's RX. CAN-transceiver logic names do not cross: MCU CAN TX connects
to the transceiver TX/D input and MCU CAN RX connects to the transceiver RX/R
output. All logic grounds must be common.

## connectionApp channel contract

The firmware matches the default `connectionApp/config.json` mapping:

| CRSF channel | Function | Behavior |
| ---: | --- | --- |
| CH1 | Steering | left/right mix |
| CH2 | Throttle | forward/reverse |
| CH3 | Drive mode | low = rear-only 2WD; center = middle+rear 4WD; high = 6WD |
| CH4 | Lights | reserved for the future lighting driver |
| CH5 | ARM | low = disarm; low-to-high edge = arm |
| CH6 | ESTOP | high latches emergency stop |

Each STM32 publishes its three measured encoder speeds on CAN at 10 Hz. ESP32
forwards alternating left/right groups as standard CRSF `0x0C` RPM telemetry,
which the connection application displays per motor. There is no per-motor
voltage measurement in the current wiring. BTS7960 current-sense values also
remain unavailable until their analog scaling has been physically calibrated.

The ESP32 accepts standard `RC_CHANNELS_PACKED` CRSF frames at 420000 baud.
Its skid-steer mixer matches the application's display:

```text
left  = throttle - steering * 0.5
right = throttle + steering * 0.5
```

The initial firmware limit is 200 RPM. ESP32 sends six independent wheel
targets in two CAN frames, and each STM32 applies its three targets separately.
The current skid-steer mixer uses the same speed for every engaged wheel on a
side; CH3 disables the unused motor-driver outputs in 2WD and 4WD. The limit
deliberately leaves margin below the motor's approximately 333 RPM no-load
rating during first tests.

## Safe start procedure

Perform the first test with all wheels raised off the ground and an accessible
physical power disconnect.

1. Keep ARM off, ESTOP off, and throttle/steering centered.
2. Power the 3.3 V logic rail, XR4 supply, and CAN nodes.
3. Start `connectionApp`, connect it to the Nomad built-in USB-UART at 400000 baud, and
   confirm that it is sending about 50 packets/s.
4. Turn on the radio link and verify the ESP32 serial log reports `RC=UP`,
   `ARM=OFF`, and reasonable CH1/CH2 values near 992 at center.
5. Toggle ARM from off to on while both controls are centered. The ESP32 logs
   `ARMED` and enables the STM32 nodes.
6. Apply a very small throttle command and verify wheel direction. Disarm
   immediately if either side is reversed.

ARM is intentionally edge-qualified: after boot or radio loss, firmware must
first observe CH5 low and then a low-to-high transition with both axes within
8% of center. Starting with CH5 already high cannot arm the vehicle.

CH6 latches ESTOP. Releasing ESTOP alone does not resume motion: set ARM low,
center the controls, keep ESTOP low, and switch ARM high again. CRSF channel
loss for more than 100 ms disarms the ESP32; missing CAN motion commands for
more than 300 ms disables each STM32 node.

## Firmware images

- ESP32: `firmware/esp32/build/ugv_esp32_aux.bin`
- Left STM32: `build/stm32-left-release/UGV_STM32_LEFT.bin`
- Right STM32: `build/stm32-right-release/UGV_STM32_RIGHT.bin`

Flashing only the ESP32 is not sufficient when the STM32 nodes still contain
the older UART-only application. Both motor nodes need their matching current
image before CAN radio commands can move the vehicle.
