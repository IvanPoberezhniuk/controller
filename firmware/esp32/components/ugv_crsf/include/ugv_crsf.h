#ifndef UGV_CRSF_H
#define UGV_CRSF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UGV_CRSF_CHANNEL_COUNT 16u
#define UGV_CRSF_DIAGNOSTIC_FRAME_TYPE 0x80u
#define UGV_CRSF_DIAGNOSTIC_VERSION 2u
#define UGV_CRSF_DIAGNOSTIC_PAYLOAD_SIZE 60u

enum {
    UGV_CRSF_DIAG_FLAG_RF_LINK = 1u << 0,
    UGV_CRSF_DIAG_FLAG_ARMED = 1u << 1,
    UGV_CRSF_DIAG_FLAG_ESTOP = 1u << 2,
};

typedef enum {
    UGV_CRSF_EVENT_NONE       = 0u,
    UGV_CRSF_EVENT_CHANNELS   = 1u << 0,
    UGV_CRSF_EVENT_LINK_STATS = 1u << 1,
} ugv_crsf_event_t;

typedef struct {
    uint8_t frame[64];
    size_t frame_size;
    size_t expected_size;
    uint16_t channels[UGV_CRSF_CHANNEL_COUNT];
    uint32_t channel_frame_count;
    uint32_t crc_error_count;
    uint8_t link_quality_pct;
    int8_t rssi_dbm;
    bool link_stats_seen;
} ugv_crsf_receiver_t;

typedef struct {
    uint32_t control_tx_count;
    uint16_t control_tx_fail_count;
    uint32_t telemetry_rx_count;
    uint16_t telemetry_age_ms;
    uint16_t uart_crc_error_count;
    uint16_t uart_format_error_count;
    uint8_t safety_state;
    uint8_t fault_mask;
    uint8_t valid_mask;
    uint16_t control_rx_count;
    uint8_t last_control_flags;
    uint8_t last_enabled_mask;
} ugv_crsf_link_diagnostic_t;

typedef struct {
    uint8_t flags;
    uint8_t drive_mode;
    int16_t throttle_per_mille;
    int16_t steering_per_mille;
    uint32_t crsf_channel_frame_count;
    uint16_t crsf_crc_error_count;
    ugv_crsf_link_diagnostic_t left;
    ugv_crsf_link_diagnostic_t right;
} ugv_crsf_diagnostic_t;

void ugv_crsf_init(ugv_crsf_receiver_t *receiver);
ugv_crsf_event_t ugv_crsf_push_byte(ugv_crsf_receiver_t *receiver,
                                    uint8_t byte);

/* CRSF nominal RC range is 172..1811 with centre at 992. The returned value
 * is clamped to -1..+1 and a symmetric deadband is removed and rescaled. */
float ugv_crsf_channel_normalized(const ugv_crsf_receiver_t *receiver,
                                  unsigned channel,
                                  float deadband);

/* Builds the standard CRSF 0x0C RPM telemetry frame. rpm_source_id is the
 * index of the first value (0 = M1); up to 19 signed RPM values fit. */
size_t ugv_crsf_build_rpm_frame(uint8_t *frame, size_t capacity,
                                uint8_t rpm_source_id,
                                const int32_t *rpm, size_t count);

/* Builds a private CRSF passthrough frame (type 0x80, payload starts "UGV")
 * for the desktop control station. All multi-byte fields are little-endian. */
size_t ugv_crsf_build_diagnostic_frame(
    uint8_t *frame, size_t capacity, const ugv_crsf_diagnostic_t *diagnostic);

#endif /* UGV_CRSF_H */
