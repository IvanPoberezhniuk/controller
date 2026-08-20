# STM32 Left target

The left target uses the physical pin assignment generated from
`firmware/stm32-common/UGV_MotorNode.ioc` and the logical configuration in
`firmware/stm32-left/node_config.h`.

Logical motors:

| Firmware index | Wheel |
| --- | --- |
| motor0 / front | Front-left |
| motor1 / center | Center-left |
| motor2 / rear | Rear-left |

The local CD74HC4067 maps R_IS/L_IS for the three motors to channels 0-5 and
the three motor-temperature inputs to channels 6-8. Final S0-S3, SIG, and
FDCAN pins must be assigned in CubeMX before current/CAN hardware bring-up.
See `docs/wiring.md` for the shared motor/encoder pin table, multiplexer
channels, connector colors, and the signals that remain `TBD CubeMX`.
