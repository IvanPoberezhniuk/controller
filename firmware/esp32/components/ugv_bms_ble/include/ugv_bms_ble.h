#ifndef UGV_BMS_BLE_H
#define UGV_BMS_BLE_H

#include <stdbool.h>
#include <stdint.h>

/* Minimal, practical telemetry set decoded from the JK-BD4A8S6P BMS.
 * Phase (a) of this component only establishes the BLE link and logs the
 * discovered GATT table; all fields below the `connected`/`last_frame_age_ms`
 * pair stay at zero until phase (b) adds JK frame parsing. */
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
    int8_t   temp_high_c;
    int8_t   temp_low_c;
    uint16_t alarm_bits;
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
