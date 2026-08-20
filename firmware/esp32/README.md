# UGV ESP32 auxiliary node

Independent ESP-IDF project for the Sixspan ESP32-S3-N16R8 (16 MB flash,
8 MB octal PSRAM). This project is new; `F:\work\academy\blinkESP32` remains
an untouched reference only.

Responsibilities:

- Classic CAN/TWAI link through an external 3.3 V CAN transceiver;
- RadioMaster XR4 receiver over full-duplex CRSF at 420000 baud;
- SH1106 128x64 OLED and rotary encoder UI;
- IMU, ambient-light sensor, and GPS;
- vehicle lighting and a local warning buzzer.

Full audio and the camera remain Raspberry Pi responsibilities.

Build from an initialized ESP-IDF shell:

```powershell
idf.py -C firmware/esp32 set-target esp32s3
idf.py -C firmware/esp32 build
```

See `docs/pinout-esp32.md` before wiring. Peripheral implementations are
intentionally added one at a time after board/TWAI bring-up.
