#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ugv_can_codec.h"
#include "ugv_can_protocol.h"
#include "ugv_crsf.h"
#include "ugv_esp32_board.h"
#include "ugv_manual_control.h"

static const char *TAG = "ugv_control";
static twai_node_handle_t s_twai_node;
static bool s_twai_recovering;
/* ESP-IDF's node API queues pointers instead of copying frames. Keep the
 * frame and its data alive until the driver confirms that transmission is
 * complete, including no-ACK and bus-off cases. */
static twai_frame_t s_twai_tx_frame;
static uint8_t s_twai_tx_payload[8];
static bool s_twai_tx_pending;

#define RC_UART                 UART_NUM_1
#define CONTROL_PERIOD_MS       20u
#define STATUS_LOG_PERIOD_MS    1000u

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000u);
}

static esp_err_t radio_init(const ugv_esp32_board_config_t *board)
{
    const uart_config_t config = {
        .baud_rate = (int)UGV_CRSF_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_param_config(RC_UART, &config), TAG,
                        "CRSF UART configuration failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(RC_UART, board->crsf_tx, board->crsf_rx,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "CRSF UART pin configuration failed");
    return uart_driver_install(RC_UART, 1024, 0, 0, NULL, 0);
}

static esp_err_t can_init(const ugv_esp32_board_config_t *board)
{
    const twai_onchip_node_config_t config = {
        .io_cfg = {
            .tx = board->can_tx,
            .rx = board->can_rx,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = UGV_CAN_BITRATE_BPS,
        },
        .fail_retry_cnt = 1,
        .tx_queue_depth = 8u,
    };

    ESP_RETURN_ON_ERROR(twai_new_node_onchip(&config, &s_twai_node), TAG,
                        "TWAI node creation failed");
    return twai_node_enable(s_twai_node);
}

static bool can_send(uint16_t identifier, const uint8_t *payload, uint8_t size)
{
    if (payload == NULL || size > 8u) {
        return false;
    }

    twai_node_status_t status;
    if (twai_node_get_info(s_twai_node, &status, NULL) != ESP_OK) {
        return false;
    }
    if (status.state == TWAI_ERROR_BUS_OFF) {
        if (!s_twai_recovering &&
            twai_node_recover(s_twai_node) == ESP_OK) {
            s_twai_recovering = true;
            ESP_LOGW(TAG, "TWAI bus-off; recovery started");
        }
        return false;
    }
    if (status.state == TWAI_ERROR_ACTIVE && s_twai_recovering) {
        s_twai_recovering = false;
        ESP_LOGI(TAG, "TWAI bus recovered");
    }

    if (s_twai_tx_pending) {
        if (twai_node_transmit_wait_all_done(s_twai_node, 0) != ESP_OK) {
            return false;
        }
        s_twai_tx_pending = false;
    }

    memcpy(s_twai_tx_payload, payload, size);
    s_twai_tx_frame = (twai_frame_t){
        .header.id = identifier,
        .buffer = s_twai_tx_payload,
        .buffer_len = size,
    };
    if (twai_node_transmit(s_twai_node, &s_twai_tx_frame, 2) != ESP_OK) {
        return false;
    }
    s_twai_tx_pending = true;
    if (twai_node_transmit_wait_all_done(s_twai_node, 5) != ESP_OK) {
        return false;
    }
    s_twai_tx_pending = false;
    return true;
}

static bool send_control_frames(const ugv_manual_control_t *control,
                                uint8_t *sequence)
{
    uint8_t payload[8] = {0};
    const uint8_t current_sequence = (*sequence)++;
    const uint8_t enabled_mask = control->armed
                                     ? control->wheel_enable_mask
                                     : 0u;
    const ugv_can_wheel_targets_t left = {
        .sequence = current_sequence,
        .enabled_mask = enabled_mask,
        .front_rpm = control->armed ? control->left_rpm[0] : 0,
        .center_rpm = control->armed ? control->left_rpm[1] : 0,
        .rear_rpm = control->armed ? control->left_rpm[2] : 0,
    };
    const ugv_can_wheel_targets_t right = {
        .sequence = current_sequence,
        .enabled_mask = enabled_mask,
        .front_rpm = control->armed ? control->right_rpm[0] : 0,
        .center_rpm = control->armed ? control->right_rpm[1] : 0,
        .rear_rpm = control->armed ? control->right_rpm[2] : 0,
    };
    const ugv_can_system_enable_t enable = {
        .enabled = control->armed ? 1u : 0u,
        .emergency_stop = control->emergency_stop_latched ? 1u : 0u,
    };

    bool ok = ugv_can_encode_wheel_targets(payload, sizeof(payload), &left) &&
              can_send(UGV_CAN_MSG_WHEEL_TARGETS_LEFT, payload,
                       UGV_CAN_WHEEL_TARGETS_LEFT_DLC);
    memset(payload, 0, sizeof(payload));
    ok = ugv_can_encode_wheel_targets(payload, sizeof(payload), &right) &&
         can_send(UGV_CAN_MSG_WHEEL_TARGETS_RIGHT, payload,
                  UGV_CAN_WHEEL_TARGETS_RIGHT_DLC) && ok;
    memset(payload, 0, sizeof(payload));
    ok = ugv_can_encode_system_enable(payload, sizeof(payload), &enable) &&
         can_send(UGV_CAN_MSG_SYSTEM_ENABLE, payload,
                  UGV_CAN_SYSTEM_ENABLE_DLC) && ok;

    return ok;
}

void app_main(void)
{
    const ugv_esp32_board_config_t *board = ugv_esp32_board_config();
    ESP_LOGI(TAG, "UGV manual control booting");
    ESP_LOGI(TAG, "CRSF CH1=steering CH2=throttle CH3=2/4/6WD CH5=arm CH6=estop, max=%.0f RPM",
             (double)UGV_RC_MAX_RPM);
    ESP_LOGI(TAG, "TWAI TX=%d RX=%d at %u bit/s",
             board->can_tx, board->can_rx, UGV_CAN_BITRATE_BPS);
    ESP_LOGI(TAG, "CRSF TX=%d RX=%d at %u baud",
             board->crsf_tx, board->crsf_rx, UGV_CRSF_BAUD_RATE);

    ESP_ERROR_CHECK(radio_init(board));
    ESP_ERROR_CHECK(can_init(board));

    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);
    uint8_t can_sequence = 0u;
    uint32_t last_control_ms = 0u;
    uint32_t last_log_ms = 0u;
    uint32_t can_tx_errors = 0u;
    uint8_t rx[128];

    while (true) {
        const int received = uart_read_bytes(RC_UART, rx, sizeof(rx),
                                             pdMS_TO_TICKS(2));
        const uint32_t current_ms = now_ms();
        for (int i = 0; i < received; ++i) {
            const ugv_crsf_event_t event = ugv_crsf_push_byte(&radio, rx[i]);
            if ((event & UGV_CRSF_EVENT_CHANNELS) != 0u) {
                ugv_manual_control_note_channels(&control, current_ms);
            }
            if ((event & UGV_CRSF_EVENT_LINK_STATS) != 0u) {
                ugv_manual_control_note_link_stats(&control, current_ms);
            }
        }

        const bool was_armed = control.armed;
        ugv_manual_control_update(&control, &radio, current_ms);
        if (control.armed && !was_armed) {
            ESP_LOGI(TAG, "ARMED");
        } else if (!control.armed && was_armed) {
            ESP_LOGW(TAG, "DISARMED");
        }
        if ((current_ms - last_control_ms) >= CONTROL_PERIOD_MS) {
            last_control_ms = current_ms;
            if (!send_control_frames(&control, &can_sequence)) {
                can_tx_errors++;
            }
        }

        if ((current_ms - last_log_ms) >= STATUS_LOG_PERIOD_MS) {
            last_log_ms = current_ms;
            ESP_LOGI(TAG,
                     "RC=%s ARM=%s ESTOP=%s MODE=%u CH1=%u CH2=%u CH3=%u CH5=%u CH6=%u steer=%.2f throttle=%.2f L=%d/%d/%d R=%d/%d/%d LQ=%u CANerr=%lu CRCerr=%lu",
                     control.link_up ? "OK" : "LOST",
                     control.armed ? "ON" : "OFF",
                     control.emergency_stop_latched ? "ON" : "OFF",
                     control.drive_mode,
                     radio.channels[UGV_RC_STEERING_CHANNEL],
                     radio.channels[UGV_RC_THROTTLE_CHANNEL],
                     radio.channels[UGV_RC_DRIVE_MODE_CHANNEL],
                     radio.channels[UGV_RC_ARM_CHANNEL],
                     radio.channels[UGV_RC_ESTOP_CHANNEL],
                     (double)control.steering, (double)control.throttle,
                     control.left_rpm[0], control.left_rpm[1],
                     control.left_rpm[2], control.right_rpm[0],
                     control.right_rpm[1], control.right_rpm[2],
                     radio.link_quality_pct, (unsigned long)can_tx_errors,
                     (unsigned long)radio.crc_error_count);
        }
        /* The default ESP-IDF tick is coarser than 1 ms, so pdMS_TO_TICKS(1)
         * can become zero and starve IDLE0. One scheduler tick still keeps
         * the 20 ms control cadence and guarantees watchdog idle time. */
        vTaskDelay(1);
    }
}
