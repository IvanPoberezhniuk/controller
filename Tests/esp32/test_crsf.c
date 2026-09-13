#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ugv_crsf.h"

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

static void pack_channels(uint8_t payload[22], const uint16_t channels[16])
{
    memset(payload, 0, 22u);
    uint32_t accumulator = 0u;
    unsigned bits = 0u;
    size_t destination = 0u;

    for (unsigned channel = 0; channel < 16u; ++channel) {
        accumulator |= (uint32_t)(channels[channel] & 0x07ffu) << bits;
        bits += 11u;
        while (bits >= 8u) {
            payload[destination++] = (uint8_t)(accumulator & 0xffu);
            accumulator >>= 8u;
            bits -= 8u;
        }
    }
}

static ugv_crsf_event_t feed(ugv_crsf_receiver_t *receiver,
                             const uint8_t *frame, size_t size)
{
    ugv_crsf_event_t events = UGV_CRSF_EVENT_NONE;
    for (size_t i = 0; i < size; ++i) {
        events = (ugv_crsf_event_t)(events |
            ugv_crsf_push_byte(receiver, frame[i]));
    }
    return events;
}

static void test_channels_and_normalization(void)
{
    ugv_crsf_receiver_t receiver;
    ugv_crsf_init(&receiver);

    uint16_t channels[16];
    for (unsigned i = 0; i < 16u; ++i) {
        channels[i] = 992u;
    }
    channels[0] = 172u;
    channels[2] = 1811u;

    uint8_t frame[26] = {0xc8u, 24u, 0x16u};
    pack_channels(&frame[3], channels);
    frame[25] = crc8_dvb_s2(&frame[2], 23u);

    const uint8_t leading_noise[] = {0x01u, 24u, 0x16u, 0xaau, 0x55u};
    assert(feed(&receiver, leading_noise, sizeof(leading_noise)) ==
           UGV_CRSF_EVENT_NONE);
    assert(receiver.crc_error_count == 0u);

    assert(feed(&receiver, frame, sizeof(frame)) == UGV_CRSF_EVENT_CHANNELS);
    assert(receiver.channel_frame_count == 1u);
    assert(receiver.channels[0] == 172u);
    assert(receiver.channels[1] == 992u);
    assert(receiver.channels[2] == 1811u);
    assert(ugv_crsf_channel_normalized(&receiver, 0u, 0.05f) == -1.0f);
    assert(ugv_crsf_channel_normalized(&receiver, 1u, 0.05f) == 0.0f);
    assert(ugv_crsf_channel_normalized(&receiver, 2u, 0.05f) == 1.0f);

    frame[25] ^= 1u;
    assert(feed(&receiver, frame, sizeof(frame)) == UGV_CRSF_EVENT_NONE);
    assert(receiver.channel_frame_count == 1u);
    assert(receiver.crc_error_count == 1u);

    frame[25] ^= 1u;
    assert(feed(&receiver, frame, sizeof(frame)) == UGV_CRSF_EVENT_CHANNELS);
    assert(receiver.channel_frame_count == 2u);
}

static void test_link_statistics(void)
{
    ugv_crsf_receiver_t receiver;
    ugv_crsf_init(&receiver);

    uint8_t frame[14] = {
        0xc8u, 12u, 0x14u,
        55u, 45u, 87u, 3u, 1u, 2u, 3u, 70u, 60u, 2u,
        0u,
    };
    frame[13] = crc8_dvb_s2(&frame[2], 11u);

    assert(feed(&receiver, frame, sizeof(frame)) ==
           UGV_CRSF_EVENT_LINK_STATS);
    assert(receiver.link_stats_seen);
    assert(receiver.link_quality_pct == 87u);
    assert(receiver.rssi_dbm == -45);
}

static void test_rpm_telemetry_frame(void)
{
    const int32_t rpm[3] = {333, -120, 0};
    uint8_t frame[16] = {0};
    const size_t size = ugv_crsf_build_rpm_frame(frame, sizeof(frame),
                                                  3u, rpm, 3u);
    assert(size == 14u);
    assert(frame[0] == 0xc8u);
    assert(frame[1] == 12u);
    assert(frame[2] == 0x0cu);
    assert(frame[3] == 3u);
    assert(frame[4] == 0x00u && frame[5] == 0x01u && frame[6] == 0x4du);
    assert(frame[7] == 0xffu && frame[8] == 0xffu && frame[9] == 0x88u);
    assert(frame[10] == 0u && frame[11] == 0u && frame[12] == 0u);
    assert(frame[13] == crc8_dvb_s2(&frame[2], 11u));
    assert(ugv_crsf_build_rpm_frame(frame, 5u, 0u, rpm, 3u) == 0u);
}

int main(void)
{
    test_channels_and_normalization();
    test_link_statistics();
    test_rpm_telemetry_frame();
    puts("all CRSF tests passed");
    return 0;
}
