#ifndef UGV_CAN_CODEC_H
#define UGV_CAN_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ugv_can_protocol.h"

bool ugv_can_encode_motion_cmd(uint8_t *payload, size_t size,
                               const ugv_can_motion_cmd_t *message);
bool ugv_can_decode_motion_cmd(ugv_can_motion_cmd_t *message,
                               const uint8_t *payload, size_t size);

bool ugv_can_encode_system_enable(uint8_t *payload, size_t size,
                                  const ugv_can_system_enable_t *message);
bool ugv_can_decode_system_enable(ugv_can_system_enable_t *message,
                                  const uint8_t *payload, size_t size);

bool ugv_can_encode_aux_lighting(uint8_t *payload, size_t size,
                                 const ugv_can_aux_lighting_t *message);
bool ugv_can_decode_aux_lighting(ugv_can_aux_lighting_t *message,
                                 const uint8_t *payload, size_t size);

bool ugv_can_encode_control_status(uint8_t *payload, size_t size,
                                   const ugv_can_control_status_t *message);
bool ugv_can_decode_control_status(ugv_can_control_status_t *message,
                                   const uint8_t *payload, size_t size);

bool ugv_can_encode_motor_telemetry(uint8_t *payload, size_t size,
                                    const ugv_can_motor_telemetry_t *message);
bool ugv_can_decode_motor_telemetry(ugv_can_motor_telemetry_t *message,
                                    const uint8_t *payload, size_t size);

bool ugv_can_encode_fault_report(uint8_t *payload, size_t size,
                                 const ugv_can_fault_report_t *message);
bool ugv_can_decode_fault_report(ugv_can_fault_report_t *message,
                                 const uint8_t *payload, size_t size);

bool ugv_can_encode_heartbeat(uint8_t *payload, size_t size,
                              const ugv_can_heartbeat_t *message);
bool ugv_can_decode_heartbeat(ugv_can_heartbeat_t *message,
                              const uint8_t *payload, size_t size);

#endif /* UGV_CAN_CODEC_H */
