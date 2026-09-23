# ESP32 ↔ STM32 UART protocol

Both private motor links use 115200 baud, 8N1, 3.3 V logic. ESP32 sends one
control frame per link every 20 ms; STM32 sends telemetry at 10 Hz.

## Frame

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | sync `0xA5` |
| 1 | 1 | sync `0x5A` |
| 2 | 1 | protocol version `1` |
| 3 | 1 | message type |
| 4 | 1 | sequence, wraps at 255 |
| 5 | 1 | payload length, maximum 16 |
| 6 | N | payload |
| 6+N | 2 | CRC-16-CCITT, little-endian |

CRC uses polynomial `0x1021`, initial value `0xFFFF`, no reflection, over
version/type/sequence/length/payload. A streaming parser discards invalid
frames and searches for the next sync pair.

## Control (`type=1`, payload 9 bytes)

| Offset | Field |
| ---: | --- |
| 0 | target role: `1=Left`, `2=Right` |
| 1 | enable bits: bit0 front, bit1 center, bit2 rear |
| 2 | flags: bit0 ARM, bit1 ESTOP, bit2 CLEAR_FAULT |
| 3..4 | front target RPM, signed little-endian int16 |
| 5..6 | center target RPM |
| 7..8 | rear target RPM |

Each STM32 accepts only its compiled role. The configured RPM limit is checked
before any target is applied. Only a complete CRC-valid frame refreshes the
300 ms command watchdog.

CLEAR_FAULT takes priority over ARM within the same frame: a frame with
CLEAR_FAULT set only clears a latched `SAFETY_STATE_FAULT`/`DEGRADED` and
returns the node to `DISABLED` (see `safety_clear_fault()`) — it never arms
by itself, so a separate later frame with ARM set is required to resume
driving. This is the only way to leave `FAULT` short of a physical reset;
`FAULT` is entered automatically when the 300 ms command watchdog expires
while armed (e.g. the PC control app disconnects or restarts).

## Telemetry (`type=2`, payload 16 bytes)

| Offset | Field |
| ---: | --- |
| 0 | source role |
| 1 | STM32 safety state |
| 2..7 | front, center, rear measured RPM as three int16 values |
| 8..13 | front, center, rear current in mA as three uint16 values |
| 14 | per-wheel fault mask |
| 15 | bits0..2 encoder valid; bits4..6 current valid |

Current fields are zero and their validity bits are clear until the physical
BTS7960 sense scaling is calibrated.
