# STM32 Right target

The right target uses the physical pin assignment generated from
`firmware/stm32-common/UGV_MotorNode.ioc` and the logical configuration in
`firmware/stm32-right/node_config.h`.

Logical motors:

| Firmware index | Wheel |
| --- | --- |
| motor0 / front | Front-right |
| motor1 / center | Center-right |
| motor2 / rear | Rear-right |

The local CD74HC4067 maps R_IS/L_IS for the three motors to channels 0-5 and
the three motor-temperature inputs to channels 6-8. Motor and encoder signs
remain neutral defaults until verified on the assembled right drivetrain.
Final S0-S3, SIG, and FDCAN pins must be assigned in CubeMX before
current/CAN hardware bring-up.
See `docs/wiring.md` for the shared motor/encoder pin table, multiplexer
channels, connector colors, and the signals that remain `TBD CubeMX`.
