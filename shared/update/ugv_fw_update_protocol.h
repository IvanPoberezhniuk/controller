#ifndef UGV_FW_UPDATE_PROTOCOL_H
#define UGV_FW_UPDATE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ugv_can_protocol.h"

/* Firmware-update traffic uses Classic CAN 2.0 frames so a temporary
 * SocketCAN service host, STM32 FDCAN peripherals, and ESP32 TWAI can share
 * one bus during maintenance. */
#define UGV_FW_PROTOCOL_VERSION       1u
#define UGV_FW_FRAME_DLC              8u
#define UGV_FW_DATA_BYTES_PER_FRAME   6u
#define UGV_FW_ACK_INTERVAL_FRAMES    32u
#define UGV_FW_BROADCAST_NODE         0xffu

/* Service host -> STM32 command; node-specific data streams avoid spending
 * one of the eight Classic-CAN bytes on a target field. Status IDs identify
 * the sender. */
#define UGV_FW_CAN_ID_COMMAND       0x600u
#define UGV_FW_CAN_ID_DATA_LEFT     0x610u
#define UGV_FW_CAN_ID_DATA_RIGHT    0x611u
#define UGV_FW_CAN_ID_STATUS_LEFT   0x680u
#define UGV_FW_CAN_ID_STATUS_RIGHT  0x681u

typedef enum {
    /* ENTER is consumed by the running motor application, which first makes
     * outputs safe, writes the SRAM boot request, then resets. */
    UGV_FW_COMMAND_ENTER    = 0x01,
    UGV_FW_COMMAND_QUERY    = 0x02,
    UGV_FW_COMMAND_BEGIN    = 0x10, /* value = image size in bytes */
    UGV_FW_COMMAND_FINISH   = 0x11, /* value = expected image CRC-32 */
    UGV_FW_COMMAND_ACTIVATE = 0x12,
    UGV_FW_COMMAND_ABORT    = 0x13,
} ugv_fw_command_opcode_t;

typedef enum {
    UGV_FW_STATUS_IDLE     = 0x00,
    UGV_FW_STATUS_READY    = 0x01,
    UGV_FW_STATUS_ACK      = 0x02,
    UGV_FW_STATUS_VERIFIED = 0x03,
    UGV_FW_STATUS_ERROR    = 0x80,
} ugv_fw_status_code_t;

typedef enum {
    UGV_FW_ERROR_NONE             = 0,
    UGV_FW_ERROR_BAD_COMMAND      = 1,
    UGV_FW_ERROR_BAD_SESSION      = 2,
    UGV_FW_ERROR_BAD_STATE        = 3,
    UGV_FW_ERROR_IMAGE_SIZE       = 4,
    UGV_FW_ERROR_SEQUENCE         = 5,
    UGV_FW_ERROR_FLASH_ERASE      = 6,
    UGV_FW_ERROR_FLASH_WRITE      = 7,
    UGV_FW_ERROR_INCOMPLETE_IMAGE = 8,
    UGV_FW_ERROR_CRC_MISMATCH     = 9,
    UGV_FW_ERROR_METADATA_WRITE   = 10,
} ugv_fw_error_t;

/* Command frame bytes: opcode, target node, session, flags, value[31:0]. */
typedef struct {
    uint8_t opcode;
    uint8_t target_node;
    uint8_t session;
    uint8_t flags;
    uint32_t value;
} ugv_fw_command_t;

/* Data frame bytes: sequence[15:0], six firmware bytes. The final frame is
 * padded with 0xff; BEGIN's exact image size excludes that padding. */
typedef struct {
    uint16_t sequence;
    uint8_t bytes[UGV_FW_DATA_BYTES_PER_FRAME];
} ugv_fw_data_t;

/* Status frame bytes: code, session, detail, protocol version, value[31:0].
 * ACK/ERROR value is the next expected sequence; VERIFIED value is CRC-32. */
typedef struct {
    uint8_t code;
    uint8_t session;
    uint8_t detail;
    uint8_t protocol_version;
    uint32_t value;
} ugv_fw_status_t;

bool ugv_fw_encode_command(uint8_t *payload, size_t size,
                           const ugv_fw_command_t *message);
bool ugv_fw_decode_command(ugv_fw_command_t *message,
                           const uint8_t *payload, size_t size);

bool ugv_fw_encode_data(uint8_t *payload, size_t size,
                        const ugv_fw_data_t *message);
bool ugv_fw_decode_data(ugv_fw_data_t *message,
                        const uint8_t *payload, size_t size);

bool ugv_fw_encode_status(uint8_t *payload, size_t size,
                          const ugv_fw_status_t *message);
bool ugv_fw_decode_status(ugv_fw_status_t *message,
                          const uint8_t *payload, size_t size);

bool ugv_fw_data_id_for_node(uint8_t node_id, uint16_t *can_id);
bool ugv_fw_status_id_for_node(uint8_t node_id, uint16_t *can_id);

#endif /* UGV_FW_UPDATE_PROTOCOL_H */
