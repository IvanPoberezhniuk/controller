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
the three motor-temperature inputs to channels 6-8. The final planned control
pins are PA6=`MUX_SIG`, PA7=`S0`, PB2=`S1`, PB12=`S2`, PB13=`S3`. FDCAN uses
PA11=RX and PA12=TX; center LPWM moves to PB8/TIM16_CH1 and center enables move
to PA4/PA5. These assignments still require the manual CubeMX regeneration.

The role-specific bootloader is `UGV_BOOTLOADER_LEFT.bin`; its CAN data/status
IDs are `0x610`/`0x680`. See `docs/wiring.md` for the full shared pin/color
table and `docs/firmware-update.md` for provisioning and OTA commands.
