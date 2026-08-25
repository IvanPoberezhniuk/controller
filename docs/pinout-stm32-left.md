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

The six R_IS/L_IS signals connect directly to PA6, PA7, PA4, PA5, PB2, and
PB12 as documented in `wiring.md`; no analog multiplexer is used. Each driver's
R_EN and L_EN inputs share one GPIO: PB0 front, PB9 center, PB10 rear. FDCAN
uses PA11=RX and PA12=TX, and center LPWM moves to PB8/TIM16_CH1. The FDCAN,
TIM16, and common-enable labels still require manual CubeMX regeneration.

The role-specific bootloader is `UGV_BOOTLOADER_LEFT.bin`; its CAN data/status
IDs are `0x610`/`0x680`. See `docs/wiring.md` for the full shared pin/color
table and `docs/firmware-update.md` for provisioning and OTA commands.
