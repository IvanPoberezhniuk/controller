#include <stdbool.h>
#include <stdint.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ugv_bms_ble.h"
#include "ugv_crsf.h"
#include "ugv_esp32_board.h"
#include "ugv_manual_control.h"
#include "ugv_uart_protocol.h"
#include "stm32_uart_updater.h"

static const char *TAG = "ugv_control";

#define RC_UART             UART_NUM_1
#define LEFT_MOTOR_UART     UART_NUM_0
#define RIGHT_MOTOR_UART    UART_NUM_2
#define RPM_RADIO_PERIOD_MS 200u
#define DIAGNOSTIC_RADIO_PERIOD_MS 1000u
#define UPDATE_BUTTON_GPIO GPIO_NUM_0
#define UPDATE_BUTTON_HOLD_MS 2000u

typedef struct {
    uart_port_t port;
    uint8_t node_role;
    uint8_t tx_sequence;
    uint32_t control_tx_count;
    uint32_t control_tx_fail_count;
    ugv_uart_parser_t parser;
} motor_link_t;

typedef struct {
    int32_t rpm[6];
    uint32_t received_ms[2];
    uint32_t frame_count[2];
    uint8_t safety_state[2];
    uint8_t fault_mask[2];
    uint8_t valid_mask[2];
    uint16_t control_rx_count[2];
    uint8_t last_control_flags[2];
    uint8_t last_enabled_mask[2];
    bool side_seen[2];
} motor_telemetry_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000u);
}

static esp_err_t uart_link_init(uart_port_t port, gpio_num_t tx,
                                gpio_num_t rx, uint32_t baud_rate)
{
    const uart_config_t config = {
        .baud_rate = (int)baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_is_driver_installed(port)) {
        ESP_RETURN_ON_ERROR(uart_driver_delete(port), TAG,
                            "UART driver reset failed");
    }
    ESP_RETURN_ON_ERROR(uart_param_config(port, &config), TAG,
                        "UART configuration failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE), TAG,
                        "UART pin configuration failed");
    return uart_driver_install(port, 1024, 0, 0, NULL, 0);
}

static esp_err_t radio_init(const ugv_esp32_board_config_t *board)
{
    return uart_link_init(RC_UART, board->crsf_tx, board->crsf_rx,
                          UGV_CRSF_BAUD_RATE);
}

static bool motor_link_send(motor_link_t *link,
                            const ugv_manual_control_t *control,
                            const int16_t rpm[3], uint8_t enable_mask)
{
    uint8_t payload[UGV_UART_CONTROL_PAYLOAD_SIZE];
    uint8_t frame[UGV_UART_MAX_FRAME_SIZE];
    const ugv_uart_control_t message = {
        .target_role = link->node_role,
        .enabled_mask = control->armed ? enable_mask : 0u,
        .flags = (control->armed ? UGV_UART_CONTROL_FLAG_ARM : 0u) |
                 (control->emergency_stop_latched
                      ? UGV_UART_CONTROL_FLAG_ESTOP : 0u),
        .front_rpm = control->armed ? rpm[0] : 0,
        .center_rpm = control->armed ? rpm[1] : 0,
        .rear_rpm = control->armed ? rpm[2] : 0,
    };
    if (!ugv_uart_encode_control(payload, sizeof(payload), &message)) {
        return false;
    }
    const size_t frame_size = ugv_uart_encode_frame(
        frame, sizeof(frame), UGV_UART_MSG_CONTROL, link->tx_sequence++,
        payload, sizeof(payload));
    const bool sent = frame_size != 0u &&
        uart_write_bytes(link->port, frame, frame_size) == (int)frame_size;
    if (sent) {
        link->control_tx_count++;
    } else {
        link->control_tx_fail_count++;
    }
    return sent;
}

static void motor_link_receive(motor_link_t *link,
                               motor_telemetry_t *telemetry,
                               uint32_t current_ms)
{
    uint8_t bytes[64];
    int received;
    while ((received = uart_read_bytes(link->port, bytes, sizeof(bytes), 0)) > 0) {
        for (int index = 0; index < received; ++index) {
            ugv_uart_frame_t frame;
            if (!ugv_uart_parser_push(&link->parser, bytes[index], &frame) ||
                frame.type != UGV_UART_MSG_TELEMETRY) {
                continue;
            }
            ugv_uart_telemetry_t decoded;
            if (!ugv_uart_decode_telemetry(&decoded, frame.payload,
                                           frame.payload_size) ||
                decoded.node_role != link->node_role) {
                continue;
            }
            const unsigned side = link->node_role == UGV_NODE_CODE_LEFT ? 0u : 1u;
            const unsigned offset = side * 3u;
            telemetry->rpm[offset] = decoded.front_rpm;
            telemetry->rpm[offset + 1u] = decoded.center_rpm;
            telemetry->rpm[offset + 2u] = decoded.rear_rpm;
            telemetry->received_ms[side] = current_ms;
            telemetry->frame_count[side]++;
            telemetry->safety_state[side] = decoded.safety_state;
            telemetry->fault_mask[side] = decoded.fault_mask;
            telemetry->valid_mask[side] = decoded.valid_mask;
            telemetry->control_rx_count[side] = decoded.control_rx_count;
            telemetry->last_control_flags[side] = decoded.last_control_flags;
            telemetry->last_enabled_mask[side] = decoded.last_enabled_mask;
            telemetry->side_seen[side] = true;
        }
    }
}

static bool radio_send_rpm_side(const motor_telemetry_t *telemetry,
                                unsigned side, uint32_t current_ms)
{
    if (side > 1u || !telemetry->side_seen[side] ||
        (current_ms - telemetry->received_ms[side]) > 500u) {
        return false;
    }
    uint8_t frame[16] = {0};
    const unsigned offset = side * 3u;
    const size_t size = ugv_crsf_build_rpm_frame(
        frame, sizeof(frame), (uint8_t)offset, &telemetry->rpm[offset], 3u);
    return size > 0u && uart_write_bytes(RC_UART, frame, size) == (int)size;
}

static uint16_t as_u16_saturated(uint32_t value)
{
    return value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static int16_t normalized_as_per_mille(float value)
{
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    return (int16_t)(value * 1000.0f);
}

static ugv_crsf_link_diagnostic_t link_diagnostic(
    const motor_link_t *link, const motor_telemetry_t *telemetry,
    unsigned side, uint32_t current_ms)
{
    const uint32_t age = telemetry->side_seen[side]
        ? current_ms - telemetry->received_ms[side]
        : UINT16_MAX;
    return (ugv_crsf_link_diagnostic_t) {
        .control_tx_count = link->control_tx_count,
        .control_tx_fail_count = as_u16_saturated(
            link->control_tx_fail_count),
        .telemetry_rx_count = telemetry->frame_count[side],
        .telemetry_age_ms = as_u16_saturated(age),
        .uart_crc_error_count = as_u16_saturated(
            link->parser.crc_error_count),
        .uart_format_error_count = as_u16_saturated(
            link->parser.format_error_count),
        .safety_state = telemetry->safety_state[side],
        .fault_mask = telemetry->fault_mask[side],
        .valid_mask = telemetry->valid_mask[side],
        .control_rx_count = telemetry->control_rx_count[side],
        .last_control_flags = telemetry->last_control_flags[side],
        .last_enabled_mask = telemetry->last_enabled_mask[side],
    };
}

static bool radio_send_diagnostic(
    const ugv_crsf_receiver_t *radio, const ugv_manual_control_t *control,
    const motor_link_t *left_link, const motor_link_t *right_link,
    const motor_telemetry_t *telemetry, uint32_t current_ms)
{
    const ugv_crsf_diagnostic_t diagnostic = {
        .flags = (control->link_up ? UGV_CRSF_DIAG_FLAG_RF_LINK : 0u) |
                 (control->armed ? UGV_CRSF_DIAG_FLAG_ARMED : 0u) |
                 (control->emergency_stop_latched
                      ? UGV_CRSF_DIAG_FLAG_ESTOP : 0u),
        .drive_mode = control->drive_mode,
        .throttle_per_mille = normalized_as_per_mille(control->throttle),
        .steering_per_mille = normalized_as_per_mille(control->steering),
        .crsf_channel_frame_count = radio->channel_frame_count,
        .crsf_crc_error_count = as_u16_saturated(radio->crc_error_count),
        .left = link_diagnostic(left_link, telemetry, 0u, current_ms),
        .right = link_diagnostic(right_link, telemetry, 1u, current_ms),
    };
    uint8_t frame[64];
    const size_t size = ugv_crsf_build_diagnostic_frame(
        frame, sizeof(frame), &diagnostic);
    return size > 0u && uart_write_bytes(RC_UART, frame, size) == (int)size;
}

void app_main(void)
{
    const ugv_esp32_board_config_t *board = ugv_esp32_board_config();
    ESP_LOGI(TAG, "UGV dual-UART manual control booting");
    ESP_LOGI(TAG, "LEFT UART TX=%d RX=%d, RIGHT UART TX=%d RX=%d at %u baud",
             board->left_uart_tx, board->left_uart_rx,
             board->right_uart_tx, board->right_uart_rx, UGV_UART_BAUD_RATE);
    ESP_LOGI(TAG, "CRSF TX=%d RX=%d at %u baud",
             board->crsf_tx, board->crsf_rx, UGV_CRSF_BAUD_RATE);

    ESP_ERROR_CHECK(radio_init(board));

    /* BMS BLE link runs on its own NimBLE host task and never touches the
     * UARTs above; main loop only ever reads its state via
     * ugv_bms_ble_get_state(), never calls BLE APIs directly. */
    ugv_bms_ble_start();

    /* UART0 is the left motor link at runtime. Disable application logs before
     * remapping it so text can never corrupt binary motor commands. The ESP
     * ROM downloader still uses GPIO43/44 before this application starts. */
    esp_log_level_set("*", ESP_LOG_NONE);
    ESP_ERROR_CHECK(uart_link_init(LEFT_MOTOR_UART, board->left_uart_tx,
                                   board->left_uart_rx, UGV_UART_BAUD_RATE));
    ESP_ERROR_CHECK(uart_link_init(RIGHT_MOTOR_UART, board->right_uart_tx,
                                   board->right_uart_rx, UGV_UART_BAUD_RATE));

    motor_link_t left_link = {
        .port = LEFT_MOTOR_UART,
        .node_role = UGV_NODE_CODE_LEFT,
    };
    motor_link_t right_link = {
        .port = RIGHT_MOTOR_UART,
        .node_role = UGV_NODE_CODE_RIGHT,
    };
    ugv_uart_parser_init(&left_link.parser);
    ugv_uart_parser_init(&right_link.parser);

    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);
    motor_telemetry_t telemetry = {0};
    uint32_t last_control_ms = 0u;
    uint32_t last_rpm_radio_ms = 0u;
    uint32_t last_diagnostic_radio_ms = 0u;
    unsigned rpm_radio_side = 0u;
    uint8_t rx[128];
    uint32_t update_button_pressed_ms = 0u;

    const gpio_config_t update_button = {
        .pin_bit_mask = 1ULL << UPDATE_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&update_button));

    while (true) {
        const int received = uart_read_bytes(RC_UART, rx, sizeof(rx),
                                             pdMS_TO_TICKS(2));
        const uint32_t current_ms = now_ms();
        motor_link_receive(&left_link, &telemetry, current_ms);
        motor_link_receive(&right_link, &telemetry, current_ms);

        for (int index = 0; index < received; ++index) {
            const ugv_crsf_event_t event = ugv_crsf_push_byte(&radio, rx[index]);
            if ((event & UGV_CRSF_EVENT_CHANNELS) != 0u) {
                ugv_manual_control_note_channels(&control, current_ms);
            }
            if ((event & UGV_CRSF_EVENT_LINK_STATS) != 0u) {
                ugv_manual_control_note_link_stats(&control, current_ms);
            }
        }
        ugv_manual_control_update(&control, &radio, current_ms);

        if (gpio_get_level(UPDATE_BUTTON_GPIO) == 0) {
            if (update_button_pressed_ms == 0u) {
                update_button_pressed_ms = current_ms;
            } else if ((current_ms - update_button_pressed_ms) >=
                       UPDATE_BUTTON_HOLD_MS) {
                /* Send explicit DISARM frames before UART0 leaves the left
                 * motor link. Both STM32 nodes remain fail-safe throughout
                 * maintenance mode. */
                control.armed = false;
                control.emergency_stop_latched = false;
                for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
                    (void)motor_link_send(&left_link, &control,
                                          control.left_rpm, 0u);
                    (void)motor_link_send(&right_link, &control,
                                          control.right_rpm, 0u);
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                stm32_uart_updater_run(board);
            }
        } else {
            update_button_pressed_ms = 0u;
        }

        if ((current_ms - last_control_ms) >= UGV_UART_COMMAND_PERIOD_MS) {
            last_control_ms = current_ms;
            (void)motor_link_send(&left_link, &control, control.left_rpm,
                                  control.left_enable_mask);
            (void)motor_link_send(&right_link, &control, control.right_rpm,
                                  control.right_enable_mask);
        }

        if ((current_ms - last_rpm_radio_ms) >= RPM_RADIO_PERIOD_MS) {
            last_rpm_radio_ms = current_ms;
            (void)radio_send_rpm_side(&telemetry, rpm_radio_side, current_ms);
            rpm_radio_side ^= 1u;
        }
        if ((current_ms - last_diagnostic_radio_ms) >=
            DIAGNOSTIC_RADIO_PERIOD_MS) {
            last_diagnostic_radio_ms = current_ms;
            (void)radio_send_diagnostic(
                &radio, &control, &left_link, &right_link,
                &telemetry, current_ms);
        }
        vTaskDelay(1);
    }
}
