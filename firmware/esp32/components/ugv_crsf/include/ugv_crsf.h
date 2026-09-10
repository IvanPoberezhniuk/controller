#ifndef UGV_CRSF_H
#define UGV_CRSF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UGV_CRSF_CHANNEL_COUNT 16u

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

void ugv_crsf_init(ugv_crsf_receiver_t *receiver);
ugv_crsf_event_t ugv_crsf_push_byte(ugv_crsf_receiver_t *receiver,
                                    uint8_t byte);

/* CRSF nominal RC range is 172..1811 with centre at 992. The returned value
 * is clamped to -1..+1 and a symmetric deadband is removed and rescaled. */
float ugv_crsf_channel_normalized(const ugv_crsf_receiver_t *receiver,
                                  unsigned channel,
                                  float deadband);

#endif /* UGV_CRSF_H */
