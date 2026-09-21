#include "stm32_uart_updater.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ugv_crc32.h"
#include "ugv_flash_layout.h"
#include "ugv_fw_update_protocol.h"
#include "ugv_uart_protocol.h"

#define HOST_UART            UART_NUM_0
#define RIGHT_UART           UART_NUM_2
#define HOST_TX_GPIO         GPIO_NUM_43
#define HOST_RX_GPIO         GPIO_NUM_44
#define HOST_BAUD_RATE       115200u
#define UPDATE_HEADER_MAGIC  0x46564755u /* bytes "UGVF" */
#define UPDATE_HOST_VERSION  1u
#define UPDATE_IO_TIMEOUT_MS 300000u
#define UPDATE_STATUS_TIMEOUT_MS 2000u
#define UPDATE_FRAME_PACING_MS 2u
#define UPDATE_ACK_RETRIES 3u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t target_role;
    uint16_t reserved;
    uint32_t image_size;
    uint32_t image_crc32;
} update_header_t;

_Static_assert(sizeof(update_header_t) == 16u,
               "host update header must be 16 bytes");

typedef struct {
    uart_port_t port;
    uint8_t node_id;
    uint8_t tx_sequence;
    uint8_t session;
    ugv_uart_parser_t parser;
} update_link_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000u);
}

static bool configure_uart(uart_port_t port, gpio_num_t tx, gpio_num_t rx)
{
    if (uart_is_driver_installed(port)) {
        if (uart_driver_delete(port) != ESP_OK) {
            return false;
        }
    }
    const uart_config_t config = {
        .baud_rate = HOST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    return uart_param_config(port, &config) == ESP_OK &&
           uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE,
                        UART_PIN_NO_CHANGE) == ESP_OK &&
           uart_driver_install(port, 2048, 1024, 0, NULL, 0) == ESP_OK;
}

static void host_write(const char *format, ...)
{
    char line[160];
    va_list args;
    va_start(args, format);
    const int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length > 0) {
        const size_t size = (size_t)length < sizeof(line)
                                ? (size_t)length
                                : sizeof(line) - 1u;
        (void)uart_write_bytes(HOST_UART, line, size);
        (void)uart_wait_tx_done(HOST_UART, pdMS_TO_TICKS(1000));
    }
}

static bool read_exact(uart_port_t port, uint8_t *buffer, size_t size,
                       uint32_t timeout_ms)
{
    size_t received = 0u;
    const uint32_t deadline = now_ms() + timeout_ms;
    while (received < size && (int32_t)(now_ms() - deadline) < 0) {
        const int count = uart_read_bytes(
            port, &buffer[received], size - received, pdMS_TO_TICKS(20));
        if (count > 0) {
            received += (size_t)count;
        }
    }
    return received == size;
}

static bool image_vectors_valid(const uint8_t *image, uint32_t size)
{
    if (image == NULL || size < 8u || size > UGV_APP_FLASH_MAX_SIZE) {
        return false;
    }
    uint32_t initial_sp;
    uint32_t reset_handler;
    memcpy(&initial_sp, image, sizeof(initial_sp));
    memcpy(&reset_handler, &image[4], sizeof(reset_handler));
    const uint32_t reset_address = reset_handler & ~1u;
    return initial_sp >= UGV_SRAM_BASE_ADDRESS &&
           initial_sp <= UGV_SRAM_BASE_ADDRESS + UGV_USABLE_SRAM_SIZE &&
           (initial_sp & 7u) == 0u && (reset_handler & 1u) != 0u &&
           reset_address >= UGV_APP_FLASH_ADDRESS &&
           reset_address < UGV_METADATA_FLASH_ADDRESS;
}

static bool send_frame(update_link_t *link, uint8_t type,
                       const uint8_t *payload, uint8_t payload_size)
{
    uint8_t frame[UGV_UART_MAX_FRAME_SIZE];
    const size_t frame_size = ugv_uart_encode_frame(
        frame, sizeof(frame), type, link->tx_sequence++, payload,
        payload_size);
    return frame_size != 0u &&
           uart_write_bytes(link->port, frame, frame_size) ==
               (int)frame_size;
}

static bool send_command(update_link_t *link, uint8_t opcode,
                         uint32_t value)
{
    const ugv_fw_command_t command = {
        .opcode = opcode,
        .target_node = link->node_id,
        .session = link->session,
        .flags = 0u,
        .value = value,
    };
    uint8_t payload[UGV_FW_FRAME_DLC];
    return ugv_fw_encode_command(payload, sizeof(payload), &command) &&
           send_frame(link, UGV_UART_MSG_FW_COMMAND, payload,
                      sizeof(payload));
}

static bool wait_status(update_link_t *link, ugv_fw_status_t *status,
                        uint32_t timeout_ms, bool require_session)
{
    const uint32_t deadline = now_ms() + timeout_ms;
    uint8_t bytes[64];
    while ((int32_t)(now_ms() - deadline) < 0) {
        const int count = uart_read_bytes(link->port, bytes, sizeof(bytes),
                                          pdMS_TO_TICKS(20));
        for (int index = 0; index < count; ++index) {
            ugv_uart_frame_t frame;
            if (!ugv_uart_parser_push(&link->parser, bytes[index], &frame) ||
                frame.type != UGV_UART_MSG_FW_STATUS ||
                !ugv_fw_decode_status(status, frame.payload,
                                      frame.payload_size) ||
                status->protocol_version != UGV_FW_PROTOCOL_VERSION ||
                (require_session && status->session != link->session)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

static bool enter_bootloader(update_link_t *link)
{
    for (unsigned attempt = 0u; attempt < 3u; ++attempt) {
        (void)send_command(link, UGV_FW_COMMAND_ENTER, UGV_FW_ENTER_MAGIC);
        (void)uart_wait_tx_done(link->port, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    const uint32_t deadline = now_ms() + 5000u;
    while ((int32_t)(now_ms() - deadline) < 0) {
        ugv_fw_status_t status;
        (void)send_command(link, UGV_FW_COMMAND_QUERY, 0u);
        if (wait_status(link, &status, 300u, false) &&
            status.code != UGV_FW_STATUS_ERROR) {
            return true;
        }
    }
    return false;
}

static bool begin_update(update_link_t *link, uint32_t image_size,
                         uint16_t *next_sequence)
{
    ugv_fw_status_t status;
    if (!send_command(link, UGV_FW_COMMAND_BEGIN, image_size) ||
        !wait_status(link, &status, 10000u, true) ||
        status.code == UGV_FW_STATUS_ERROR ||
        (status.code != UGV_FW_STATUS_READY &&
         status.code != UGV_FW_STATUS_ACK)) {
        return false;
    }
    *next_sequence = status.code == UGV_FW_STATUS_ACK
                         ? (uint16_t)status.value
                         : 0u;
    return true;
}

static bool send_image(update_link_t *link, const uint8_t *image,
                       uint32_t image_size, uint16_t sequence)
{
    const uint32_t frame_count =
        (image_size + UGV_FW_DATA_BYTES_PER_FRAME - 1u) /
        UGV_FW_DATA_BYTES_PER_FRAME;
    unsigned no_progress_count = 0u;
    while ((uint32_t)sequence < frame_count) {
        /* Keep bursts short: the STM32 programs its application flash from
         * the same bank, so a large window can overrun its polling receiver. */
        const uint16_t window_start = sequence;
        const uint32_t window_end =
            ((uint32_t)sequence + 4u < frame_count)
                ? (uint32_t)sequence + 4u
                : frame_count;
        while ((uint32_t)sequence < window_end) {
            ugv_fw_data_t data = {.sequence = sequence};
            memset(data.bytes, 0xff, sizeof(data.bytes));
            const uint32_t offset =
                (uint32_t)sequence * UGV_FW_DATA_BYTES_PER_FRAME;
            const uint32_t remaining = image_size - offset;
            const size_t count = remaining < UGV_FW_DATA_BYTES_PER_FRAME
                                     ? remaining
                                     : UGV_FW_DATA_BYTES_PER_FRAME;
            memcpy(data.bytes, &image[offset], count);
            uint8_t payload[UGV_FW_FRAME_DLC];
            if (!ugv_fw_encode_data(payload, sizeof(payload), &data) ||
                !send_frame(link, UGV_UART_MSG_FW_DATA, payload,
                            sizeof(payload))) {
                return false;
            }
            ++sequence;

            /* The STM32 bootloader programs the same flash bank from which it
             * executes. Briefly pace frames so flash-program stalls cannot
             * overrun its polling UART receiver. */
            vTaskDelay(pdMS_TO_TICKS(UPDATE_FRAME_PACING_MS));
        }
        (void)uart_wait_tx_done(link->port, pdMS_TO_TICKS(1000));
        ugv_fw_status_t status = {0};
        bool acknowledged = false;
        for (unsigned attempt = 0u; attempt < UPDATE_ACK_RETRIES;
             ++attempt) {
            if (send_command(link, UGV_FW_COMMAND_QUERY, 0u) &&
                wait_status(link, &status, UPDATE_STATUS_TIMEOUT_MS, true)) {
                acknowledged = true;
                break;
            }
        }
        if (!acknowledged ||
            (status.code != UGV_FW_STATUS_ACK &&
             !(status.code == UGV_FW_STATUS_ERROR &&
               status.detail == UGV_FW_ERROR_SEQUENCE)) ||
            status.value > UINT16_MAX) {
            return false;
        }

        /* A missing byte can make the bootloader reject every later frame in
         * the current window. Those error replies all contain the same next
         * expected sequence. Discard the stale replies before retransmitting
         * from that sequence, otherwise the sender can loop forever on an old
         * status already queued in the ESP UART driver. */
        uart_flush_input(link->port);
        const uint16_t acknowledged_sequence = (uint16_t)status.value;
        if (acknowledged_sequence < window_start ||
            acknowledged_sequence > sequence) {
            return false;
        }
        if (acknowledged_sequence == window_start) {
            if (++no_progress_count >= UPDATE_ACK_RETRIES) {
                return false;
            }
        } else {
            no_progress_count = 0u;
        }
        sequence = acknowledged_sequence;
    }
    return true;
}

static bool finish_update(update_link_t *link, uint32_t expected_crc)
{
    ugv_fw_status_t status;
    if (!send_command(link, UGV_FW_COMMAND_FINISH, expected_crc) ||
        !wait_status(link, &status, 5000u, true) ||
        status.code != UGV_FW_STATUS_VERIFIED ||
        status.value != expected_crc ||
        !send_command(link, UGV_FW_COMMAND_ACTIVATE, 0u)) {
        return false;
    }

    /* UART0 is reused for the left motor link and the PC USB bridge. Ensure
     * ACTIVATE has physically left the FIFO before configure_uart() deletes
     * the motor-link driver and remaps UART0 back to GPIO43/GPIO44. */
    return uart_wait_tx_done(link->port, pdMS_TO_TICKS(1000)) == ESP_OK;
}

static bool perform_update(update_link_t *link, const uint8_t *image,
                           uint32_t image_size, uint32_t image_crc)
{
    uart_flush_input(link->port);
    ugv_uart_parser_init(&link->parser);
    if (!enter_bootloader(link)) {
        return false;
    }
    uint16_t sequence = 0u;
    return begin_update(link, image_size, &sequence) &&
           send_image(link, image, image_size, sequence) &&
           finish_update(link, image_crc);
}

_Noreturn void stm32_uart_updater_run(
    const ugv_esp32_board_config_t *board)
{
    if (board == NULL ||
        !configure_uart(HOST_UART, HOST_TX_GPIO, HOST_RX_GPIO)) {
        esp_restart();
    }

    host_write("UGV-UPDATER READY v1\r\n");
    update_header_t header;
    if (!read_exact(HOST_UART, (uint8_t *)&header, sizeof(header),
                    UPDATE_IO_TIMEOUT_MS) ||
        header.magic != UPDATE_HEADER_MAGIC ||
        header.version != UPDATE_HOST_VERSION || header.reserved != 0u ||
        (header.target_role != UGV_NODE_CODE_LEFT &&
         header.target_role != UGV_NODE_CODE_RIGHT) ||
        header.image_size < 8u ||
        header.image_size > UGV_APP_FLASH_MAX_SIZE) {
        host_write("ERROR invalid header\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    uint8_t *image = heap_caps_malloc(header.image_size, MALLOC_CAP_8BIT);
    if (image == NULL) {
        host_write("ERROR no memory\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    host_write("SEND %lu\r\n", (unsigned long)header.image_size);
    if (!read_exact(HOST_UART, image, header.image_size,
                    UPDATE_IO_TIMEOUT_MS) ||
        ugv_crc32(image, header.image_size) != header.image_crc32 ||
        !image_vectors_valid(image, header.image_size)) {
        free(image);
        host_write("ERROR invalid image or CRC\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    host_write("BUFFERED\r\n");

    const bool left = header.target_role == UGV_NODE_CODE_LEFT;
    update_link_t link = {
        .port = left ? HOST_UART : RIGHT_UART,
        .node_id = left ? 0x10u : 0x11u,
        .session = (uint8_t)((esp_random() % 255u) + 1u),
    };

    if (left) {
        if (!configure_uart(HOST_UART, board->left_uart_tx,
                            board->left_uart_rx)) {
            free(image);
            esp_restart();
        }
    } else if (!configure_uart(RIGHT_UART, board->right_uart_tx,
                               board->right_uart_rx)) {
        free(image);
        host_write("ERROR right UART\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    const bool success = perform_update(&link, image, header.image_size,
                                        header.image_crc32);
    free(image);

    if (left) {
        (void)configure_uart(HOST_UART, HOST_TX_GPIO, HOST_RX_GPIO);
    }
    host_write(success ? "OK STM32 updated\r\n"
                       : "ERROR STM32 update failed\r\n");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}
