#ifndef UGV_BOOT_CAN_H
#define UGV_BOOT_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "ugv_fw_update_protocol.h"

typedef struct {
    uint16_t identifier;
    uint8_t payload[UGV_FW_FRAME_DLC];
} ugv_boot_can_frame_t;

bool ugv_boot_can_init(uint8_t node_id);
bool ugv_boot_can_receive(ugv_boot_can_frame_t *frame);
bool ugv_boot_can_send_status(uint8_t node_id,
                              const ugv_fw_status_t *status);

#endif /* UGV_BOOT_CAN_H */
