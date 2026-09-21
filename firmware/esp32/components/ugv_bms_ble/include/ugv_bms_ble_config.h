#ifndef UGV_BMS_BLE_CONFIG_H
#define UGV_BMS_BLE_CONFIG_H

/* JK-BD4A8S6P BLE identity for this specific vehicle's pack.
 *
 * MAC is hardware-specific to this BMS unit (from the vendor label / JK app).
 * Address bytes below are little-endian (LSB first), i.e. reversed from the
 * usual colon-separated "c8:47:80:55:04:4f" notation, to match NimBLE's
 * ble_addr_t.val[] byte order. */
#define UGV_BMS_BLE_MAC_ADDR   { 0x4f, 0x04, 0x55, 0x80, 0x47, 0xc8 }
#define UGV_BMS_BLE_ADDR_TYPE  BLE_ADDR_PUBLIC /* RECOMMENDED guess -- verify on bench, try BLE_ADDR_RANDOM if connect fails */

/* Device serial/password, used as AES-128 key material if this unit's
 * firmware encrypts BLE frames (common on newer JK "BD" boards). Whether
 * encryption is actually active here is UNCONFIRMED -- determine on the
 * bench in phase (b): if raw notification bytes don't start with the
 * expected plaintext JK header (0x55 0xAA 0xEB 0x90), assume this applies. */
#define UGV_BMS_BLE_DEVICE_PASSWORD "60126554516"

/* CONFIRMED on bench (2026-09-21) via phase (a) GATT enumeration against this
 * exact JK-BD4A8S6P unit at UGV_BMS_BLE_MAC_ADDR. This is NOT the commonly
 * documented JK 0xFFE0/0xFFE1 16-bit profile -- this unit's BLE module uses a
 * TI SimpleBLE-style 128-bit UUID base (f000ffc0-0451-4000-b000-000000000000),
 * typical of a CC254x/CC26xx-class BLE radio. The 0xFFE0 service that IS
 * present on this device has no characteristics in its handle range and is
 * not used. See README.md for the full logged GATT table. */
#define UGV_BMS_BLE_SERVICE_UUID128 \
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, \
                     0x00, 0x40, 0x51, 0x04, 0xc0, 0xff, 0x00, 0xf0)
#define UGV_BMS_BLE_CHAR1_UUID128 \
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, \
                     0x00, 0x40, 0x51, 0x04, 0xc1, 0xff, 0x00, 0xf0)
#define UGV_BMS_BLE_CHAR2_UUID128 \
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, \
                     0x00, 0x40, 0x51, 0x04, 0xc2, 0xff, 0x00, 0xf0)
/* Both characteristics advertise write + write-without-response + notify
 * (props 0x1c); which direction is actually used for which is unconfirmed --
 * phase (b) determines this empirically (subscribe to both, write a JK
 * status-request command to each, see which one talks back). */

#endif /* UGV_BMS_BLE_CONFIG_H */
