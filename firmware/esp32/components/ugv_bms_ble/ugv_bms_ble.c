#include "ugv_bms_ble.h"
#include "ugv_bms_ble_config.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nimble/nimble_npl.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"

static const char *TAG = "ugv_bms_ble";

#define JK_FRAME_SIZE              300U
#define JK_FRAME_BUFFER_SIZE       400U
#define JK_COMMAND_SIZE             20U
#define JK_COMMAND_CELL_INFO      0x96U
#define JK_COMMAND_DEVICE_INFO    0x97U
#define JK_REQUEST_SPACING_MS       450U

static const ble_addr_t s_bms_addr = {
    .type = UGV_BMS_BLE_ADDR_TYPE,
    .val = UGV_BMS_BLE_MAC_ADDR,
};

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_service_start_handle;
static uint16_t s_service_end_handle;
static uint16_t s_data_handle;
static uint16_t s_cccd_handle;
static bool s_gatt_ready;

static uint8_t s_frame[JK_FRAME_BUFFER_SIZE];
static size_t s_frame_len;
static int64_t s_last_frame_us;

static struct ble_npl_callout s_request_callout;
static uint8_t s_request_stage;

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static ugv_bms_state_t s_state = {
    .last_frame_age_ms = UINT32_MAX,
};

/* Provided by NimBLE's default persistence-store implementation. */
void ble_store_config_init(void);

static void bms_connect(void);
static int bms_gap_event(struct ble_gap_event *event, void *arg);

static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int8_t tenths_c_to_i8(const uint8_t *p)
{
    int32_t value = (int16_t)get_u16_le(p) / 10;
    if (value > INT8_MAX) {
        value = INT8_MAX;
    } else if (value < INT8_MIN) {
        value = INT8_MIN;
    }
    return (int8_t)value;
}

static void bms_reset_link_state(void)
{
    s_service_start_handle = 0;
    s_service_end_handle = 0;
    s_data_handle = 0;
    s_cccd_handle = 0;
    s_gatt_ready = false;
    s_request_stage = 0;
    s_frame_len = 0;
    ble_npl_callout_stop(&s_request_callout);
}

static void set_connected(bool connected)
{
    portENTER_CRITICAL(&s_state_lock);
    s_state.connected = connected;
    if (!connected) {
        s_last_frame_us = 0;
        s_state.last_frame_age_ms = UINT32_MAX;
    }
    portEXIT_CRITICAL(&s_state_lock);
}

bool ugv_bms_ble_get_state(ugv_bms_state_t *out)
{
    if (out == NULL) {
        return false;
    }

    portENTER_CRITICAL(&s_state_lock);
    *out = s_state;
    const int64_t last_frame_us = s_last_frame_us;
    portEXIT_CRITICAL(&s_state_lock);

    if (out->connected && last_frame_us != 0) {
        const uint64_t age_ms = (uint64_t)(esp_timer_get_time() - last_frame_us) / 1000U;
        out->last_frame_age_ms = age_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)age_ms;
    }
    return true;
}

static void build_jk_command(uint8_t command, uint8_t frame[JK_COMMAND_SIZE])
{
    memset(frame, 0, JK_COMMAND_SIZE);
    frame[0] = 0xaa;
    frame[1] = 0x55;
    frame[2] = 0x90;
    frame[3] = 0xeb;
    frame[4] = command;

    uint8_t checksum = 0;
    for (size_t i = 0; i < JK_COMMAND_SIZE - 1; ++i) {
        checksum = (uint8_t)(checksum + frame[i]);
    }
    frame[JK_COMMAND_SIZE - 1] = checksum;
}

static int send_jk_command(uint8_t command)
{
    if (!s_gatt_ready || s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_data_handle == 0) {
        return BLE_HS_ENOTCONN;
    }

    uint8_t frame[JK_COMMAND_SIZE];
    build_jk_command(command, frame);
    const int rc = ble_gattc_write_no_rsp_flat(s_conn_handle, s_data_handle,
                                                frame, sizeof(frame));
    if (rc == 0) {
        ESP_LOGI(TAG, "Sent JK command 0x%02x to FFE1 handle=0x%04x",
                 command, s_data_handle);
    } else {
        ESP_LOGW(TAG, "Failed to send JK command 0x%02x; rc=%d", command, rc);
    }
    return rc;
}

static void bms_request_timer(struct ble_npl_event *event)
{
    (void)event;
    if (!s_gatt_ready) {
        return;
    }

    if (s_request_stage == 0) {
        /* 0x96 + 0x97 enables the BMS's own periodic telemetry stream.
         * These are one-shot read requests; repeatedly sending 0x96 can make
         * some JK hardware acknowledge every request with its buzzer. */
        (void)send_jk_command(JK_COMMAND_CELL_INFO);
        s_request_stage = 1;
        ble_npl_callout_reset(&s_request_callout,
                              ble_npl_time_ms_to_ticks32(JK_REQUEST_SPACING_MS));
    } else if (s_request_stage == 1) {
        (void)send_jk_command(JK_COMMAND_DEVICE_INFO);
        s_request_stage = 2;
    }
}

static void parse_jk02_32s_cell_info(const uint8_t *frame)
{
    /* JK02 32S is the format used by V15H/V15.41. Its fields after the cell
     * arrays are shifted by 32 bytes compared with the older 24S frame. */
    const uint32_t enabled_cells = get_u32_le(&frame[70]);
    uint16_t cell_min = UINT16_MAX;
    uint16_t cell_max = 0;
    uint16_t cell_mv[4] = {0, 0, 0, 0};
    uint8_t cell_count = 0;

    for (uint8_t i = 0; i < 32; ++i) {
        const uint16_t mv = get_u16_le(&frame[6U + (size_t)i * 2U]);
        if ((enabled_cells & (1UL << i)) == 0 || mv == 0) {
            continue;
        }
        if (mv < cell_min) {
            cell_min = mv;
        }
        if (mv > cell_max) {
            cell_max = mv;
        }
        if (cell_count < 4U) {
            cell_mv[cell_count++] = mv;
        }
    }
    if (cell_min == UINT16_MAX) {
        cell_min = 0;
    }

    const int8_t temp1 = tenths_c_to_i8(&frame[162]);
    const int8_t temp2 = tenths_c_to_i8(&frame[164]);
    const uint32_t cycles = get_u32_le(&frame[182]);

    /* Offsets below are the JK02_32S variants (base offset + 32, matching the
     * cell-array/aggregate-field split already confirmed by the fields
     * above) of the community-documented JK02_24S layout: balancing action
     * at 140, charge/discharge MOSFET enables at 166/167, charger-plugged at
     * 213 (see syssi/esphome-jk-bms jk_bms_ble.cpp). Not yet bench-verified
     * against this specific BMS -- cross-check against the official JK app
     * after flashing. */
    const uint8_t balancer_status = frame[172];
    const bool charging_enabled = frame[198] != 0;
    const bool discharging_enabled = frame[199] != 0;
    const bool charger_plugged = frame[245] != 0;

    portENTER_CRITICAL(&s_state_lock);
    s_state.pack_voltage_v = (float)get_u32_le(&frame[150]) * 0.001f;
    s_state.pack_current_a = (float)(int32_t)get_u32_le(&frame[158]) * 0.001f;
    s_state.soc_pct = frame[173];
    s_state.remaining_capacity_ah = (float)get_u32_le(&frame[174]) * 0.001f;
    s_state.full_capacity_ah = (float)get_u32_le(&frame[178]) * 0.001f;
    s_state.cycle_count = cycles > UINT16_MAX ? UINT16_MAX : (uint16_t)cycles;
    s_state.cell_mv_min = cell_min;
    s_state.cell_mv_max = cell_max;
    s_state.cell_mv_delta = cell_max >= cell_min ? cell_max - cell_min : 0;
    memcpy(s_state.cell_mv, cell_mv, sizeof(cell_mv));
    s_state.temp_low_c = temp1 < temp2 ? temp1 : temp2;
    s_state.temp_high_c = temp1 > temp2 ? temp1 : temp2;
    s_state.alarm_bits = get_u32_le(&frame[166]);
    s_state.charging_enabled = charging_enabled;
    s_state.discharging_enabled = discharging_enabled;
    s_state.charger_plugged = charger_plugged;
    s_state.balancer_status = balancer_status;
    s_last_frame_us = esp_timer_get_time();
    s_state.last_frame_age_ms = 0;
    portEXIT_CRITICAL(&s_state_lock);

    ESP_LOGI(TAG,
             "BMS %.3fV %.3fA SOC=%u%% cells=%u-%umV delta=%umV temp=%d..%dC alarms=0x%08lx",
             s_state.pack_voltage_v, s_state.pack_current_a, s_state.soc_pct,
             s_state.cell_mv_min, s_state.cell_mv_max, s_state.cell_mv_delta,
             s_state.temp_low_c, s_state.temp_high_c,
             (unsigned long)s_state.alarm_bits);
}

static void process_complete_frame(void)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < JK_FRAME_SIZE - 1; ++i) {
        checksum = (uint8_t)(checksum + s_frame[i]);
    }
    if (checksum != s_frame[JK_FRAME_SIZE - 1]) {
        ESP_LOGW(TAG, "Dropped JK frame: checksum 0x%02x != 0x%02x",
                 checksum, s_frame[JK_FRAME_SIZE - 1]);
        return;
    }

    const uint8_t type = s_frame[4];
    ESP_LOGI(TAG, "Valid JK frame type=0x%02x counter=%u", type, s_frame[5]);
    if (type == 0x02) {
        parse_jk02_32s_cell_info(s_frame);
    } else {
        portENTER_CRITICAL(&s_state_lock);
        s_last_frame_us = esp_timer_get_time();
        s_state.last_frame_age_ms = 0;
        portEXIT_CRITICAL(&s_state_lock);
    }
}

static void consume_notification(const uint8_t *data, size_t len)
{
    if (len >= 4 && data[0] == 0x55 && data[1] == 0xaa &&
        data[2] == 0xeb && data[3] == 0x90) {
        s_frame_len = 0;
    } else if (s_frame_len == 0) {
        ESP_LOGD(TAG, "Ignoring non-JK notification on FFE1 (%u bytes)",
                 (unsigned)len);
        return;
    }

    if (len > sizeof(s_frame) - s_frame_len) {
        ESP_LOGW(TAG, "Dropped oversized JK frame");
        s_frame_len = 0;
        return;
    }

    memcpy(&s_frame[s_frame_len], data, len);
    s_frame_len += len;
    if (s_frame_len >= JK_FRAME_SIZE) {
        process_complete_frame();
        s_frame_len = 0;
    }
}

static int bms_cccd_write_cb(uint16_t conn_handle,
                             const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)attr;
    (void)arg;
    if (error->status != 0) {
        ESP_LOGE(TAG, "Failed to enable FFE1 notifications; status=%d",
                 error->status);
        return 0;
    }

    s_gatt_ready = true;
    s_request_stage = 0;
    ESP_LOGI(TAG, "FFE1 notifications enabled; JK link ready");
    ble_npl_callout_reset(&s_request_callout, ble_npl_time_ms_to_ticks32(100));
    return 0;
}

static int bms_dsc_disc_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           uint16_t chr_val_handle,
                           const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)chr_val_handle;
    (void)arg;
    if (error->status == 0) {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(0x2902)) == 0) {
            s_cccd_handle = dsc->handle;
            ESP_LOGI(TAG, "Found FFE1 CCCD handle=0x%04x", s_cccd_handle);
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "FFE1 descriptor discovery failed; status=%d",
                 error->status);
        return 0;
    }
    if (s_cccd_handle == 0) {
        ESP_LOGE(TAG, "FFE1 CCCD (0x2902) not found");
        return 0;
    }

    const uint8_t enable_notify[2] = { 0x01, 0x00 };
    const int rc = ble_gattc_write_flat(conn_handle, s_cccd_handle,
                                        enable_notify, sizeof(enable_notify),
                                        bms_cccd_write_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Could not write FFE1 CCCD; rc=%d", rc);
    }
    return 0;
}

static int bms_chr_disc_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (error->status == 0) {
        s_data_handle = chr->val_handle;
        ESP_LOGI(TAG, "Found FFE1 value handle=0x%04x properties=0x%02x",
                 s_data_handle, chr->properties);
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "FFE1 discovery failed; status=%d", error->status);
        return 0;
    }
    if (s_data_handle == 0) {
        ESP_LOGE(TAG, "FFE1 characteristic not found in FFE0 service");
        return 0;
    }

    const int rc = ble_gattc_disc_all_dscs(conn_handle, s_data_handle,
                                            s_service_end_handle,
                                            bms_dsc_disc_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Could not start FFE1 descriptor discovery; rc=%d", rc);
    }
    return 0;
}

static int bms_svc_disc_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service, void *arg)
{
    (void)arg;
    if (error->status == 0) {
        s_service_start_handle = service->start_handle;
        s_service_end_handle = service->end_handle;
        ESP_LOGI(TAG, "Found FFE0 service handles=0x%04x..0x%04x",
                 s_service_start_handle, s_service_end_handle);
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "FFE0 discovery failed; status=%d", error->status);
        return 0;
    }
    if (s_service_start_handle == 0) {
        ESP_LOGE(TAG, "FFE0 service not found");
        return 0;
    }

    const int rc = ble_gattc_disc_chrs_by_uuid(
        conn_handle, s_service_start_handle, s_service_end_handle,
        BLE_UUID16_DECLARE(UGV_BMS_BLE_CHARACTERISTIC_UUID),
        bms_chr_disc_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Could not start FFE1 discovery; rc=%d", rc);
    }
    return 0;
}

static int bms_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            bms_reset_link_state();
            set_connected(true);
            ESP_LOGI(TAG, "Connected to BMS; conn_handle=%u", s_conn_handle);
            const int rc = ble_gattc_disc_svc_by_uuid(
                s_conn_handle, BLE_UUID16_DECLARE(UGV_BMS_BLE_SERVICE_UUID),
                bms_svc_disc_cb, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Could not start FFE0 discovery; rc=%d", rc);
            }
        } else {
            ESP_LOGW(TAG, "Connect failed; status=%d, retrying",
                     event->connect.status);
            bms_connect();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "BMS disconnected; reason=%d, reconnecting",
                 event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        bms_reset_link_state();
        set_connected(false);
        bms_connect();
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        if (event->notify_rx.attr_handle != s_data_handle) {
            return 0;
        }
        uint8_t raw[256];
        uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
        if (len > sizeof(raw)) {
            ESP_LOGW(TAG, "Notification too large: %u", len);
            return 0;
        }
        if (os_mbuf_copydata(event->notify_rx.om, 0, len, raw) == 0) {
            consume_notification(raw, len);
        }
        return 0;
    }

    default:
        return 0;
    }
}

static void bms_connect(void)
{
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer own address type; rc=%d", rc);
        return;
    }

    rc = ble_gap_connect(own_addr_type, &s_bms_addr, 30000, NULL,
                         bms_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to start connect; rc=%d", rc);
    }
}

static void bms_on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE host reset; reason=%d", reason);
}

static void bms_on_sync(void)
{
    const int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "No usable BLE address; rc=%d", rc);
        return;
    }
    bms_connect();
}

static void bms_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ugv_bms_ble_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NimBLE; err=%d", err);
        return;
    }

    ble_hs_cfg.reset_cb = bms_on_reset;
    ble_hs_cfg.sync_cb = bms_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    ble_npl_callout_init(&s_request_callout, nimble_port_get_dflt_eventq(),
                         bms_request_timer, NULL);
    nimble_port_freertos_init(bms_host_task);
}
