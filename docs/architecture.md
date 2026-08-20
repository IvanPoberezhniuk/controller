# UGV controller architecture

## Runtime topology

```text
                         Classic CAN, 500 kbit/s
  Raspberry Pi 5 ───────────────┬───────────────────┬───────────────
  high-level control            │                   │
                         STM32 Left          STM32 Right       ESP32 AUX
                         3 left motors       3 right motors    UI/sensors/light
                         encoders/current    encoders/current  GPS/OLED/buzzer
                         temperatures        temperatures
                              │                   │
                         CD74HC4067          CD74HC4067
```

The motor nodes close their own speed-control and safety loops. Loss of the
Raspberry Pi or ESP32 must never leave a motor executing an old command.

## Source ownership

`firmware/stm32-common` is the only copy of generated STM32 code, HAL drivers,
motor control, safety, and platform adapters. `firmware/stm32-left` and
`firmware/stm32-right` contain target identity and calibration only. This
prevents the two motor firmwares from drifting apart while still producing
separate images.

`firmware/esp32` is an independent ESP-IDF project. It does not include or
modify the older `blinkESP32` repository.

`shared/can` is platform-neutral and is consumed by STM32, ESP32, Raspberry Pi
software, and host tests. C structures are never copied directly to CAN data;
the codec defines byte order and payload length explicitly.

## Hardware responsibility boundaries

- STM32 nodes own motor PWM/enables, quadrature encoders, local current and
  temperature sampling, command timeout, controlled stop, and driver disable.
- ESP32 owns the local display/control panel, IMU, light sensor, GPS, low-power
  lighting outputs, and warning buzzer.
- Raspberry Pi owns the IMX708 camera, full speaker/audio path, navigation,
  remote connectivity, logging, and vehicle-level command generation.

Every CAN controller requires its own external transceiver. Only the two
physical ends of the bus receive 120-ohm termination.
