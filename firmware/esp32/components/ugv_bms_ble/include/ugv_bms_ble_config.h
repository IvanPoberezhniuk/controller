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

/* Confirmed from the Android JK app connection trace on 2026-09-22:
 * service FFE0, bidirectional characteristic FFE1 (value handle 0x0012 on
 * this module), and its CCCD at 0x0013. The app writes 20-byte commands and
 * receives fragmented notifications through the same FFE1 characteristic. */
#define UGV_BMS_BLE_SERVICE_UUID         0xffe0
#define UGV_BMS_BLE_CHARACTERISTIC_UUID  0xffe1

#endif /* UGV_BMS_BLE_CONFIG_H */
