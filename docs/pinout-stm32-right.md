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

The local CD74HC4067 maps R_IS/L_IS for the three motors to channels 0-5;
channels 6-15 remain unconnected reserve. Motor and encoder signs remain
neutral defaults until verified on the assembled right drivetrain. The final
planned control pins are PA6=`MUX_SIG`, PA7=`S0`, PB2=`S1`, PB12=`S2`,
PB13=`S3`. FDCAN uses PA11=RX and PA12=TX; center LPWM moves to PB8/TIM16_CH1
and center enables move to PA4/PA5. These assignments still require the manual
CubeMX regeneration.

The role-specific bootloader is `UGV_BOOTLOADER_RIGHT.bin`; its CAN data/status
IDs are `0x611`/`0x681`. See `docs/wiring.md` for the full shared pin/color
table and `docs/firmware-update.md` for provisioning and OTA commands.
