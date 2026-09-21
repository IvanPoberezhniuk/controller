# ugv_bms_ble

NimBLE GATT client for the vehicle's JIKONG JK-BD4A8S6P BMS. Intended to
connect over BLE (the only protocol this BMS supports), decode its telemetry,
and relay it to `main.c` via `ugv_bms_ble_get_state()`, which then forwards it
to the PC control station over the existing CRSF radio link.

## Status: ON HOLD after phase (a)

Phase (a) (link + GATT enumeration) is done and works. Phase (b) (actual JK
frame parsing) is blocked on not knowing this unit's real command protocol --
see "What we tried" below. Picking this back up should start with the BLE
HCI snoop capture described in "Next step", not more blind guessing.

The component as committed only:
- brings up NimBLE (`ugv_bms_ble_start()`),
- connects directly to the configured MAC (no scanning),
- enumerates every GATT service/characteristic on connect and logs it,
- hex-dumps any unsolicited notification bytes,
- auto-reconnects on disconnect.

It sends **no commands** to the BMS. `ugv_bms_ble_get_state()` only reports
`connected`/`disconnected`; every telemetry field stays zero.

## Confirmed hardware identity

- BLE MAC: `c8:47:80:55:04:4f`
- Vendor ID (from JK app): `JK_BD4A8S6P`, Serial `60126554516`
- Hardware `V15H`, Software `V15.41` (from the JK app's "About" screen)
- Ground-truth readings from the JK app while paired directly (for later
  validation once real decoding works): pack 13.38V, 0.00A idle, SOC 99%,
  20.0Ah / 19.8Ah remaining, cells ~3.345-3.348V (very tight balance), MOS
  25.6°C, T1 24.2°C, T2 24.0°C, cycle count 0, LFP, no alarms.
- **BLE only allows one central connection at a time.** The phone app and
  the ESP32 cannot both be connected to the BMS -- disconnect the phone app
  before the ESP32 will be able to connect.

## Confirmed GATT table (phase (a) bench log, 2026-09-21)

This is **not** the commonly-documented JK "JK02" layout (`0xFFE0`/`0xFFE1`
on an ESP32-integrated radio). This unit's actual data-capable service is a
TI SimpleBLE-style 128-bit UUID, typical of a separate CC254x/BK3432-class
BLE bridge chip:

```
svc uuid=0x1800 start=1  end=9     (Generic Access)
svc uuid=0x1801 start=10 end=13    (Generic Attribute)
svc uuid=0xffe0 start=14 end=19    -- chr discovery reports ZERO characteristics
                                       in this range, yet handle 18 (inside it)
                                       is live and unsolicited (see below)
svc uuid=0x180a start=20 end=38    (Device Information)
svc uuid=0x180f start=39 end=42    (Battery Service, standard GATT one -- not
                                       BMS pack data, just the BLE module's own
                                       coin-cell/supply level if present)
svc uuid=f000ffc0-0451-4000-b000-000000000000 start=43 end=51
    chr uuid=f000ffc1-0451-4000-b000-000000000000 val_handle=45 properties=0x1c
    chr uuid=f000ffc2-0451-4000-b000-000000000000 val_handle=49 properties=0x1c
```

(`0x1c` = write + write-without-response + notify, on both f000ffc1 and
f000ffc2.)

`ugv_bms_ble_config.h`'s `UGV_BMS_BLE_SERVICE_UUID128`/`CHAR1`/`CHAR2` macros
hold these confirmed UUIDs already (not the generic 0xFFE0/0xFFE1 guess).

### The handle-18 heartbeat

Something inside the `0xffe0` service's handle range (attr_handle **18**) has
been continuously emitting an *unsolicited* notification every ~200ms since
the moment of connection, with no subscription ever requested:

```
notify attr_handle=18 len=4 data=41 54 0d 0a       <- ASCII "AT\r\n"
```

This looks like a generic BLE-UART bridge chip's own liveness heartbeat, not
BMS data. It never changed regardless of anything sent to the other
characteristics.

## What we tried (all failed to produce real telemetry)

All of this was **read-only status requests**, never a config/write command,
sent while the JK phone app was disconnected. None of it should have any
lasting effect on the BMS (the battery-protection MCU is a separate chip from
the tiny BLE bridge these experiments talk to), but none of it worked either:

1. **Bare 5-byte JK "read all" request** `AA 55 90 EB 96` to `f000ffc1` (with
   response) -> got a constant, always-identical 10-byte reply
   `0f 00 80 19 42 42 42 42 0a 00` regardless of anything about the request.
2. **Correct 20-byte JK02 frame with real checksum**, per the confirmed
   `syssi/esphome-jk-bms` implementation (`AA 55 90 EB <cmd> <len> <value LE
   x4> <pad> <checksum=sum(bytes[0..18])%256>`), commands `0x96` (cell info)
   and `0x97` (device info), sent with-response and without-response, to both
   `f000ffc1` and `f000ffc2` -> **identical** constant replies every time:
   `f000ffc1` always replies `0f 00 80 19 42 42 42 42 0a 00`, `f000ffc2`
   always replies `00 00`. A constant reply regardless of command content
   means we are not being parsed as a valid command at all by whatever is on
   the other end -- wrong protocol, wrong channel, or a missing
   auth/handshake step, not a checksum bug.
3. **Direct write to handle 18** (guessing it might be the real data channel
   despite chr discovery finding nothing there) -> the BLE link reconnected
   once and then started emitting garbled zero-length notifications for a
   while afterward. Almost certainly just destabilized the cheap BLE bridge
   chip's own firmware state (recoverable by power-cycling/reconnecting, and
   unrelated to the battery-protection MCU), but it's a clear signal **not**
   to write to unconfirmed raw handles again. Don't repeat this.

Conclusion: this specific hardware/firmware revision (`BD` series, `V15H` /
`V15.41`) does not speak the community-documented JK02 protocol on
`f000ffc1`/`f000ffc2` the way it's documented for other JK product lines, or
it requires an authentication step we don't know (the JK app's "Modify PWD.
in time" banner suggests a default password is currently in use, which may
need to be sent before the device will respond with real data).

## Next step: BLE HCI snoop log from the real JK app

Rather than continue guessing blind at an undocumented protocol variant (and
risk more of what happened in experiment 3 above), the reliable path is to
capture what the **real JK app** actually sends/receives:

1. On the phone: Settings -> Developer options -> enable "Bluetooth HCI
   snoop log".
2. Open the JK app, connect to the BMS, let the Status screen fully
   populate, then disconnect.
3. Pull the log via `adb bugreport bugreport.zip` (the log is bundled inside
   at `FS/data/misc/bluetooth/logs/btsnoop_hci.log`) -- plain file-browser
   access to `Android/data/btsnoop_hci.log` does not work on this phone's
   Android version (scoped storage).
4. Open in Wireshark to see the exact real command bytes, checksum/auth
   scheme, and response format for this exact unit.

This was on hold as of 2026-09-22 because setting up ADB got complicated
enough on this machine that it wasn't worth pushing through in the moment --
revisit this when there's time to install Android platform-tools properly
(https://developer.android.com/tool/adb) and enable USB debugging on the
phone.

## Bench bring-up procedure (for whenever this resumes)

`firmware/esp32/sdkconfig` normally ships with `CONFIG_ESP_CONSOLE_NONE=y`
(from `sdkconfig.defaults`) because UART0 is repurposed as the left-motor
link at runtime. To see `ESP_LOGI` output from this component during bench
work:

1. In `firmware/esp32/sdkconfig`, set `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`
   (plain UART0 console -- this board's ESP32-S3 does **not** expose a
   separate native USB-Serial-JTAG port to the case USB connector, only the
   CH343/UART0 bridge, so the previously-assumed "use native USB-JTAG so it
   doesn't collide with the motor link" approach does not apply here).
2. In `firmware/esp32/main/main.c`, temporarily skip the
   `esp_log_level_set("*", ESP_LOG_NONE)` call and the `LEFT_MOTOR_UART`
   `uart_link_init()` call (comment them out) so the console stays alive --
   there's no STM32 attached to that link during a pure BLE bench session
   anyway.
3. Build, flash, and read the console with a serial terminal at 115200 baud
   on the board's UART0 port.
4. **Revert both of those temporary edits before flashing any build that
   will actually run with the STM32 motor link attached.**
