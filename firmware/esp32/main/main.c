#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "ugv_can_protocol.h"
#include "ugv_esp32_board.h"

static const char *TAG = "ugv_aux";

void app_main(void)
{
    const ugv_esp32_board_config_t *board = ugv_esp32_board_config();

    ESP_LOGI(TAG, "UGV ESP32 auxiliary node booting");
    ESP_LOGI(TAG, "CAN node=0x%02x, protocol=%u.%u",
             UGV_CAN_NODE_ESP32_AUX,
             UGV_CAN_PROTOCOL_VERSION_MAJOR,
             UGV_CAN_PROTOCOL_VERSION_MINOR);
    ESP_LOGI(TAG, "TWAI TX=%d RX=%d; OLED SDA=%d SCL=%d",
             board->can_tx, board->can_rx, board->oled_sda, board->oled_scl);

    /* Hardware services are added as independent components. Keeping the
     * initial target minimal makes board and CAN bring-up testable first. */
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
