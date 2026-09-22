#ifndef UGV_BMS_BLE_H
#define UGV_BMS_BLE_H

#include <stdbool.h>
#include <stdint.h>

/* Minimal, practical telemetry set decoded from JK02 32S cell-info frames
 * emitted by the JK-BD4A8S6P V15H/V15.41 BMS. */
typedef struct {
    bool     connected;
    uint32_t last_frame_age_ms; /* UINT32_MAX if no frame has ever been decoded */
    float    pack_voltage_v;
    float    pack_current_a;       /* +charge / -discharge */
    uint8_t  soc_pct;
    float    remaining_capacity_ah;
    float    full_capacity_ah;
    uint16_t cycle_count;
    uint16_t cell_mv_min;
    uint16_t cell_mv_max;
    uint16_t cell_mv_delta;
    uint16_t cell_mv[4]; /* pack is confirmed 4S; per-cell voltage in arrival order */
    int8_t   temp_high_c;
    int8_t   temp_low_c;
    uint32_t alarm_bits;
    bool     charging_enabled;
    bool     discharging_enabled;
    bool     charger_plugged;
    uint8_t  balancer_status; /* 0 = off, 1 = charging balancer, 2 = discharging balancer */
} ugv_bms_state_t;

/* Starts NVS + the NimBLE host, and a background task that connects to the
 * configured MAC (see ugv_bms_ble_config.h), auto-reconnecting on drop.
 * Call once, before the main control loop. Never blocks. */
void ugv_bms_ble_start(void);

/* Non-blocking snapshot of the latest known BMS state. Safe to call from the
 * main super-loop every iteration; never touches BLE APIs directly.
 * Returns false if the BLE link has never connected. */
bool ugv_bms_ble_get_state(ugv_bms_state_t *out);

#endif /* UGV_BMS_BLE_H */
