# UGV ESP32 control/AUX node

ESP-IDF project for the Sixspan ESP32-S3-N16R8. It reads full-duplex CRSF
from RadioMaster XR4 at 420000 baud and owns two private 115200-baud UART links:

- UART0: Left STM32, GPIO39 TX / GPIO40 RX;
- UART2: Right STM32, GPIO41 TX / GPIO42 RX;
- UART1: XR4, GPIO38 TX / GPIO21 RX.

The application applies ARM, ESTOP, deadband, drive-mode, mixing, and 100 ms
radio timeout rules, then sends role-addressed CRC-protected commands every
20 ms. It receives RPM telemetry from both STM32 nodes and relays it to XR4.
It also relays a versioned `UGV` diagnostic frame once per second for
ConnectionApp, including RF/ARM state and health counters for both UART links.
CAN/TWAI is not used.

Build from an initialized ESP-IDF shell:

```powershell
. C:\esp\v6.0.2\esp-idf\export.ps1
idf.py -C firmware/esp32 build
```

UART0 application console output is disabled because UART0 carries the Left
motor protocol at runtime. The ROM USB-UART flashing path on GPIO43/GPIO44 is
unchanged. See `docs/pinout-esp32.md` and `docs/manual-radio-control.md`.
