#include "ugv_bms_ble.h"
#include "ugv_bms_ble_config.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"

static const char *TAG = "ugv_bms_ble";

static const ble_addr_t s_bms_addr = {
    .type = UGV_BMS_BLE_ADDR_TYPE,
    .val = UGV_BMS_BLE_MAC_ADDR,
};

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static ugv_bms_state_t s_state = {
    .last_frame_age_ms = UINT32_MAX,
};

/* Provided by the "bt" component's default persistence-store implementation
 * (nimble/host/store/config); no public header declares it. */
void ble_store_config_init(void);

static void bms_connect(void);
static int bms_gap_event(struct ble_gap_event *event, void *arg);

static void set_connected_locked(bool connected)
{
    portENTER_CRITICAL(&s_state_lock);
    s_state.connected = connected;
    portEXIT_CRITICAL(&s_state_lock);
}

bool ugv_bms_ble_get_state(ugv_bms_state_t *out)
{
    if (out == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_state_lock);
    *out = s_state;
    portEXIT_CRITICAL(&s_state_lock);
    return true;
}

/* Phase (a) only: log every characteristic of one already-discovered service.
 * This is the bench deliverable -- confirm the BMS's actual GATT layout
 * before writing anything to it. See STATUS.md for what this found on the
 * JK-BD4A8S6P and why protocol work is paused here. */
static int bms_chr_disc_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    (void)conn_handle;
    (void)arg;
    if (error->status != 0) {
        return 0; /* BLE_HS_EDONE or an error -- nothing more for this service */
    }
    char uuid_str[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&chr->uuid.u, uuid_str);
    ESP_LOGI(TAG, "    chr uuid=%s val_handle=%u properties=0x%02x",
             uuid_str, chr->val_handle, chr->properties);
    return 0;
}

static int bms_svc_disc_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service, void *arg)
{
    (void)arg;
    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "GATT service discovery complete");
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGW(TAG, "GATT service discovery error; status=%d", error->status);
        return 0;
    }

    char uuid_str[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&service->uuid.u, uuid_str);
    ESP_LOGI(TAG, "  svc uuid=%s start=%u end=%u", uuid_str,
             service->start_handle, service->end_handle);

    const int rc = ble_gattc_disc_all_chrs(conn_handle, service->start_handle,
                                           service->end_handle,
                                           bms_chr_disc_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to start characteristic discovery; rc=%d", rc);
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
            set_connected_locked(true);
            ESP_LOGI(TAG, "Connected to BMS; conn_handle=%u", s_conn_handle);
            const int rc = ble_gattc_disc_all_svcs(s_conn_handle,
                                                   bms_svc_disc_cb, NULL);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to start service discovery; rc=%d", rc);
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
        set_connected_locked(false);
        bms_connect();
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        /* Bench diagnostics only: hex-dump any unsolicited notification
         * bytes (this BMS's BLE bridge is known to send a periodic
         * heartbeat on its own, unrelated to any request we send). No
         * commands are written to the BMS by this build. */
        uint8_t raw[256];
        uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
        if (len > sizeof(raw)) {
            len = sizeof(raw);
        }
        if (os_mbuf_copydata(event->notify_rx.om, 0, len, raw) == 0) {
            char hex[sizeof(raw) * 3 + 1];
            hex[0] = '\0';
            for (uint16_t i = 0; i < len; ++i) {
                snprintf(&hex[i * 3], 4, "%02x ", raw[i]);
            }
            ESP_LOGI(TAG, "notify attr_handle=%u len=%u data=%s",
                     event->notify_rx.attr_handle, len, hex);
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
    int rc = ble_hs_util_ensure_addr(0);
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

    nimble_port_freertos_init(bms_host_task);
}
