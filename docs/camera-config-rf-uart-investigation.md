# Camera video-option control over RF — architecture investigation

**Status: investigation only. Nothing in this document is implemented.**
No firmware, wiring, or Pi service changes have been made. This is a saved
snapshot of an in-progress design discussion so it can be resumed later.

## Goal

Let the operator change the Pi camera's **video options/config** (e.g.
resolution, bitrate, exposure — not the video stream itself) from
`connectionApp` while driving, using the existing RF link instead of
building a new Wi-Fi/IP path.

Explicitly **not** in scope: sending the live video feed itself over RF.
That was considered and rejected immediately — bandwidth. HD video over
ELRS is already a documented rejected approach in `ugv-project-plan`; this
investigation does not reopen it. Video stays on Wi-Fi/RTSP
(`docs/architecture.md`, Phase 8).

## Settled direction (recommended implementation, not yet built)

```text
connectionApp --CRSF (spare AUX channel)--> ESP32 --wired UART--> Raspberry Pi
```

Command flow only: connectionApp → RF → ESP32 → UART → Pi. Not
connectionApp → Wi-Fi → Pi directly.

Why this over the two alternatives considered:

1. **connectionApp → Wi-Fi directly to Pi** — rejected for now. Would need
   new Wi-Fi/IP server code on the Pi that doesn't exist yet
   (`ugv-ip-gateway` is still Phase 6/unbuilt). The RF link already exists
   and has spare AUX channel capacity (CH7+, per `manual-radio-control.md`).
2. **connectionApp → RF → ESP32 → Wi-Fi/IP → Pi** — rejected for the same
   reason: still requires building the Wi-Fi/IP gateway server on the Pi
   side just to receive it.
3. **connectionApp → RF → ESP32 → wired UART → Pi** — **chosen**. Avoids
   writing any new network server code; ESP32 already exists as the CRSF
   endpoint, and a direct wired UART to the Pi is simple point-to-point
   serial, matching the pattern already used elsewhere in the project
   (`SerialWorker` on the PC side).

## Pin budget check (confirmed via datasheet)

Reviving CAN (ESP32 TWAI ↔ STM32, previously abandoned — see
`can-busoff-research.md`) would want GPIO17 (TX) / GPIO18 (RX), the same
pins the old CAN implementation used and which are currently free
(`pinout-esp32.md`). The question was whether a *new* ESP32↔Pi UART link
could coexist with CAN on the same ESP32 without a pin conflict.

**Resolved:** yes. Verified against the official Espressif ESP32-S3 Series
Datasheet v2.2 (Tables 3-1, 3-3, 3-4, 3-5 — strapping pins, boot mode
control, VDD_SPI voltage control, JTAG signal source control):

- **GPIO17 / GPIO18** stay reserved for CAN (TWAI TX/RX) if/when CAN is
  revived.
- **GPIO3 + GPIO46** identified as a safe alternate pair for the new
  ESP32↔Pi UART link — not strapping-critical in a way that conflicts with
  normal boot, and not otherwise allocated in the current pinout.
- GPIO0 and GPIO45 were considered and rejected for this role: GPIO0
  controls boot mode selection and GPIO45 controls VDD_SPI voltage —
  both risk interfering with normal boot/flash behavior if pulled by
  external wiring at power-up.

**Unresolved / before committing in hardware:** verify actual eFuse state
on the physical ESP32 board with `espefuse.py summary` — the datasheet
analysis is correct for default configuration, but eFuses can change a
pin's strapping behavior on a specific chip.

## CAN-on-Pi question (revisited a rejected decision, then closed)

The user asked whether the Pi could join the *same* CAN bus as ESP32/STM32
directly (not just receive relayed data). This explicitly revisits a
documented rejected approach: **"placing Raspberry Pi permanently on the
CAN trunk instead of using Wi-Fi/IP"** (`ugv-project-plan`) and CLAUDE.md
rule 7 ("Raspberry Pi is Wi-Fi-only and must not be added to the permanent
CAN trunk").

Discussion path:
1. If pursued, **listen-only SocketCAN mode** (`ip link set can0 type can
   ... listen-only on`) was recommended as the materially safer variant —
   structurally matches the Pi's real role (consumer, not producer) and
   can't disrupt the bus (no ACK transmission).
2. Proposed hardware: MCP2515 (SPI-to-CAN controller, since Pi 5 has no
   native CAN peripheral) + the already-owned SN65HVD230 transceiver.
3. **User will not buy an MCP2515.** This closed the direct-CAN-on-Pi path.

## Current recommendation (supersedes the CAN-on-Pi idea above)

**No CAN hardware on the Pi at all.** Instead: ESP32 relays CAN telemetry
to the Pi over the same UART link already being designed for camera-config
commands (GPIO3/46).

Rationale:
- ESP32 already needs its own TWAI controller once CAN is revived, and
  already owns an SN65HVD230 transceiver — no new hardware purchase.
- Keeps the Pi's "Wi-Fi-only, never a CAN node" rule intact — does not
  reopen the rejected approach, it routes around it.
- One UART link now carries two logical flows instead of needing separate
  CAN hardware on the Pi: camera-config commands (connectionApp→RF→ESP32→
  Pi) and CAN telemetry relay (ESP32 reads CAN→Pi).

This has been proposed to the user but not yet explicitly confirmed as the
final design.

## Open question: can the ESP32↔Pi UART be a single wire?

User asked whether the link can be reduced from standard TX+RX to one
wire. Key fact established: **both identified data flows on this link are
unidirectional, ESP32→Pi only** — camera-config commands and CAN-telemetry
relay both only require ESP32 to transmit and Pi to receive.

- If Pi never needs to talk back to ESP32 (no ack/handshake needed): a
  genuine single wire (ESP32 TX → Pi RX + shared GND, no return line at
  all) is straightforward and would free one of GPIO3/GPIO46 entirely.
- If Pi does need a return path: true single-wire *half-duplex* UART is
  more involved — needs open-drain-style wiring with a pull-up resistor,
  turn-taking protocol (only one side transmits at a time), and the ESP32
  side supports this natively while the Pi's standard hardware UART (PL011
  on Pi 5) does not have a native single-wire mode, so it would need
  either extra hardware (a chip — ruled out per the MCP2515 decision above)
  or software-level workarounds.

**Not yet answered by the user:** whether Pi ever needs to send anything
back to ESP32 on this link (e.g. an ack, a "config applied" confirmation,
or a request trigger), which determines whether the simple one-wire
answer applies or not.

## Status summary (per CLAUDE.md information categories)

- **Confirmed:** GPIO17/18 were the former CAN pins and are free; GPIO3/46
  pass datasheet-level safety analysis for a new UART link; MCP2515 will
  not be purchased.
- **Current design decision:** camera-config path goes
  connectionApp→RF→ESP32→UART→Pi, not Wi-Fi-direct or RF-then-Wi-Fi.
- **Recommended implementation (not yet confirmed by user):** ESP32 relays
  CAN telemetry to Pi over the same UART link instead of any CAN hardware
  on the Pi.
- **Unresolved decisions:** single-wire vs. TX+RX UART (pending whether Pi
  needs a return path); exact UART baud/framing/protocol for the
  camera-config and telemetry-relay messages; whether/when CAN is actually
  revived at all (still a separate, not-yet-restarted effort per
  `can-busoff-research.md`); eFuse verification on the physical ESP32
  board before committing GPIO3/46 in hardware.
- **Future expansion:** none identified beyond this yet.

## Next steps when resuming

1. Answer: does Pi need a return path to ESP32 on this link?
2. If confirmed, update `docs/wiring.md` with the new ESP32↔Pi UART wire(s)
   and, if CAN is being revived at the same time, the restored GPIO17/18
   CAN wiring.
3. Update `ugv-raspberry-pi` and `ugv-can-protocol` skills to record the
   ESP32-relays-telemetry-over-UART design in place of any Pi-side CAN tap.
4. Run `espefuse.py summary` on the physical ESP32 board before wiring
   GPIO3/46.
