#ifndef UGV_BOOTLOADER_H
#define UGV_BOOTLOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ugv_fw_update_protocol.h"

typedef enum {
    UGV_BOOTLOADER_IDLE = 0,
    UGV_BOOTLOADER_RECEIVING,
    UGV_BOOTLOADER_VERIFIED,
    UGV_BOOTLOADER_FAILED,
} ugv_bootloader_state_t;

/* Hardware-specific flash and reset operations. prepare() must invalidate the
 * previous application metadata before erasing/writing the application. The
 * finalize() implementation must CRC the programmed flash, then publish valid
 * metadata only if it matches expected_crc. */
typedef struct {
    void *context;
    uint32_t max_image_size;
    bool (*prepare)(void *context, uint32_t image_size);
    bool (*write)(void *context, uint32_t offset,
                  const uint8_t *data, size_t size);
    bool (*finalize)(void *context, uint32_t image_size,
                     uint32_t expected_crc);
    void (*abort)(void *context);
    void (*activate)(void *context);
} ugv_bootloader_platform_t;

typedef struct {
    uint8_t node_id;
    uint8_t session;
    ugv_bootloader_state_t state;
    uint32_t image_size;
    uint32_t received_size;
    uint32_t crc_state;
    uint16_t next_sequence;
    uint8_t last_error;
    const ugv_bootloader_platform_t *platform;
} ugv_bootloader_t;

void ugv_bootloader_init(ugv_bootloader_t *bootloader, uint8_t node_id,
                         const ugv_bootloader_platform_t *platform);

/* Returns true when a status frame was produced in response. Commands for a
 * different node are silently ignored; broadcast is accepted only for QUERY. */
bool ugv_bootloader_handle_command(ugv_bootloader_t *bootloader,
                                   const ugv_fw_command_t *command,
                                   ugv_fw_status_t *status);

/* Returns true on an ACK, recoverable NACK, completion ACK, or fatal error.
 * Most in-order data frames intentionally return false to reduce CAN traffic. */
bool ugv_bootloader_handle_data(ugv_bootloader_t *bootloader,
                                const ugv_fw_data_t *data,
                                ugv_fw_status_t *status);

#endif /* UGV_BOOTLOADER_H */
