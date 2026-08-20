# Controller interconnect

## CAN bus

Connect Raspberry Pi, STM32 Left, STM32 Right, and ESP32 AUX as one linear
CAN trunk with short stubs. Every MCU connects through a compatible CAN
transceiver; MCU pins do not connect directly to CAN-H/CAN-L.

- Nominal bitrate: 500 kbit/s.
- Twisted pair: CAN-H with CAN-L.
- Common signal ground between node power domains.
- 120-ohm termination at exactly the two physical ends of the trunk.
- Do not add termination at every node.

Use a 3.3 V logic-compatible transceiver for ESP32-S3. Confirm the logic-side
voltage of each STM32 and Raspberry Pi CAN interface before wiring.

## Power outputs

ESP32 lighting GPIOs are logic signals only. Lamps and any higher-current
buzzer require MOSFETs or dedicated drivers, flyback protection for inductive
loads, fusing, and a shared reference ground.
