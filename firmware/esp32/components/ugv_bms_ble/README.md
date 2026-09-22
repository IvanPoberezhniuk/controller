# JK BMS BLE telemetry

NimBLE client for the vehicle's JK-BD4A8S6P (`V15H` / `V15.41`) BMS.

## Data path

```text
JK BMS -- BLE --> ESP32 -- CRSF private frame 0x81 / ELRS RF --> connectionApp
```

The ESP32 connects to the configured BMS MAC, discovers service `FFE0` and
characteristic `FFE1`, enables its `2902` notification descriptor, sends the
two one-shot JK read/start-stream commands, assembles the fragmented 300-byte
JK02 32S responses, and decodes the latest cell-info frame.

Commands sent after each BLE connection:

1. `0x96` (settings/status request)
2. `0x97` (device-info request), 450 ms later

The BMS then streams cell-info frame `0x02` itself. Do not poll `0x96`
repeatedly: this BMS can acknowledge repeated requests with an audible beep.

## Confirmed GATT details

These values came from the Android JK app trace captured on 2026-09-22:

- BLE MAC: `c8:47:80:55:04:4f`
- service: `FFE0`, handles `0x000e..0x0013`
- `FFE2`: value handle `0x0010`, properties `0x04`
- bidirectional `FFE1`: value handle `0x0012`, properties `0x1c`
- notification CCCD: handle `0x0013`, value `01 00`
- command length: 20 bytes
- response length: 300 bytes, fragmented over BLE notifications

Only one BLE central can use the BMS at a time. Disconnect the JK phone app
before expecting the ESP32 to connect.

## Decoded telemetry

- BLE connection and frame age
- pack voltage/current and SOC
- remaining/nominal capacity and cycle count
- minimum, maximum, and delta cell voltage
- low/high sensor temperature
- full 32-bit JK alarm mask

The ESP32 emits this snapshot every 2 seconds as CRSF frame type `0x81` with
magic `BMS` and protocol version `1`. `connectionApp` validates its CRSF CRC,
stores the values in `TelemetryState`, displays the details in the telemetry
panel, and drives the HUD battery indicator from SOC. BMS samples are not
written to the application log.

## Safety and diagnostics

The component only sends read/start-stream commands; it does not write BMS
settings. A frame is accepted only when its header and checksum are valid.
RF telemetry marks data valid only while BLE is connected and the most recent
decoded frame is at most 10 seconds old.

Runtime logs are disabled after boot because UART0 becomes the left STM32
binary link. BMS state is therefore observed in `connectionApp`'s Telemetry
panel, not an ESP32 serial console or the application's Logs panel. If it shows
BLE disconnected, first ensure the phone app is disconnected.
