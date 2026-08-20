#ifndef UGV_CAN_CODEC_H
#define UGV_CAN_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ugv_can_protocol.h"

bool ugv_can_encode_motion_cmd(uint8_t *payload, size_t payload_size,
                               const ugv_can_motion_cmd_t *message);
bool ugv_can_decode_motion_cmd(ugv_can_motion_cmd_t *message,
                               const uint8_t *payload, size_t payload_size);

bool ugv_can_encode_system_enable(uint8_t *payload, size_t payload_size,
                                  const ugv_can_system_enable_t *message);
bool ugv_can_decode_system_enable(ugv_can_system_enable_t *message,
                                  const uint8_t *payload, size_t payload_size);

#endif /* UGV_CAN_CODEC_H */
