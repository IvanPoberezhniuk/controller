#include "ugv_crsf.h"

#include <string.h>

#define CRSF_LENGTH_MIN               2u
#define CRSF_LENGTH_MAX               62u
#define CRSF_TYPE_LINK_STATISTICS     0x14u
#define CRSF_TYPE_RC_CHANNELS_PACKED  0x16u
#define CRSF_RC_PAYLOAD_SIZE          22u
#define CRSF_LINK_STATS_SIZE          10u
#define CRSF_CHANNEL_CENTER           992u
#define CRSF_CHANNEL_MIN              172u
#define CRSF_CHANNEL_MAX              1811u

static uint8_t crc8_dvb_s2(const uint8_t *data, size_t size)
{
    uint8_t crc = 0u;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0xd5u)
                                : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static void decode_channels(ugv_crsf_receiver_t *receiver,
                            const uint8_t *payload)
{
    uint32_t accumulator = 0u;
    unsigned bits = 0u;
    size_t source = 0u;

    for (unsigned channel = 0; channel < UGV_CRSF_CHANNEL_COUNT; ++channel) {
        while (bits < 11u) {
            accumulator |= (uint32_t)payload[source++] << bits;
            bits += 8u;
        }
        receiver->channels[channel] = (uint16_t)(accumulator & 0x07ffu);
        accumulator >>= 11u;
        bits -= 11u;
    }
    receiver->channel_frame_count++;
}

void ugv_crsf_init(ugv_crsf_receiver_t *receiver)
{
    if (receiver == NULL) {
        return;
    }
    memset(receiver, 0, sizeof(*receiver));
    for (unsigned channel = 0; channel < UGV_CRSF_CHANNEL_COUNT; ++channel) {
        receiver->channels[channel] = CRSF_CHANNEL_CENTER;
    }
}

ugv_crsf_event_t ugv_crsf_push_byte(ugv_crsf_receiver_t *receiver,
                                    uint8_t byte)
{
    if (receiver == NULL) {
        return UGV_CRSF_EVENT_NONE;
    }

    if (receiver->frame_size == 0u) {
        receiver->frame[receiver->frame_size++] = byte;
        return UGV_CRSF_EVENT_NONE;
    }

    if (receiver->frame_size == 1u) {
        if (byte < CRSF_LENGTH_MIN || byte > CRSF_LENGTH_MAX) {
            receiver->frame_size = 0u;
            receiver->expected_size = 0u;
            return UGV_CRSF_EVENT_NONE;
        }
        receiver->frame[receiver->frame_size++] = byte;
        receiver->expected_size = (size_t)byte + 2u;
        return UGV_CRSF_EVENT_NONE;
    }

    receiver->frame[receiver->frame_size++] = byte;
    if (receiver->frame_size < receiver->expected_size) {
        return UGV_CRSF_EVENT_NONE;
    }

    const size_t frame_size = receiver->expected_size;
    const uint8_t length = receiver->frame[1];
    const uint8_t expected_crc = receiver->frame[frame_size - 1u];
    const uint8_t actual_crc = crc8_dvb_s2(&receiver->frame[2],
                                           (size_t)length - 1u);
    ugv_crsf_event_t event = UGV_CRSF_EVENT_NONE;

    if (actual_crc != expected_crc) {
        receiver->crc_error_count++;
    } else {
        const uint8_t type = receiver->frame[2];
        const uint8_t *payload = &receiver->frame[3];
        const size_t payload_size = (size_t)length - 2u;

        if (type == CRSF_TYPE_RC_CHANNELS_PACKED &&
            payload_size == CRSF_RC_PAYLOAD_SIZE) {
            decode_channels(receiver, payload);
            event = (ugv_crsf_event_t)(event | UGV_CRSF_EVENT_CHANNELS);
        } else if (type == CRSF_TYPE_LINK_STATISTICS &&
                   payload_size == CRSF_LINK_STATS_SIZE) {
            const uint8_t best_rssi = payload[0] < payload[1]
                                          ? payload[0]
                                          : payload[1];
            receiver->rssi_dbm = -(int8_t)best_rssi;
            receiver->link_quality_pct = payload[2] > 100u ? 100u : payload[2];
            receiver->link_stats_seen = true;
            event = (ugv_crsf_event_t)(event | UGV_CRSF_EVENT_LINK_STATS);
        }
    }

    receiver->frame_size = 0u;
    receiver->expected_size = 0u;
    return event;
}

float ugv_crsf_channel_normalized(const ugv_crsf_receiver_t *receiver,
                                  unsigned channel,
                                  float deadband)
{
    if (receiver == NULL || channel >= UGV_CRSF_CHANNEL_COUNT) {
        return 0.0f;
    }

    const uint16_t raw = receiver->channels[channel];
    float value;
    if (raw >= CRSF_CHANNEL_CENTER) {
        value = (float)(raw - CRSF_CHANNEL_CENTER) /
                (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER);
    } else {
        value = -(float)(CRSF_CHANNEL_CENTER - raw) /
                (float)(CRSF_CHANNEL_CENTER - CRSF_CHANNEL_MIN);
    }

    if (value > 1.0f) {
        value = 1.0f;
    } else if (value < -1.0f) {
        value = -1.0f;
    }

    if (deadband < 0.0f) {
        deadband = 0.0f;
    } else if (deadband > 0.5f) {
        deadband = 0.5f;
    }

    const float magnitude = value < 0.0f ? -value : value;
    if (magnitude <= deadband) {
        return 0.0f;
    }
    const float scaled = (magnitude - deadband) / (1.0f - deadband);
    return value < 0.0f ? -scaled : scaled;
}
