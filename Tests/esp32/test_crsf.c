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

static void test_diagnostic_frame(void)
{
    /* Diagnostic frame is version UGV_CRSF_DIAGNOSTIC_VERSION (3): 4-byte
     * CRSF header, 16-byte control preamble, two 28-byte link-diagnostic
     * blocks (left/right), 8 bytes of ESP32 uptime/heap, 1-byte CRC --
     * see ugv_crsf.c's ugv_crsf_build_diagnostic_frame/encode_link_diagnostic
     * for the authoritative layout this test mirrors. */
    const ugv_crsf_diagnostic_t diagnostic = {
        .flags = UGV_CRSF_DIAG_FLAG_RF_LINK | UGV_CRSF_DIAG_FLAG_ARMED,
        .drive_mode = 3u,
        .throttle_per_mille = -125,
        .steering_per_mille = 250,
        .crsf_channel_frame_count = 0x12345678u,
        .crsf_crc_error_count = 9u,
        .left = {
            .control_tx_count = 1000u,
            .control_tx_fail_count = 2u,
            .telemetry_rx_count = 90u,
            .telemetry_age_ms = 25u,
            .uart_crc_error_count = 3u,
            .uart_format_error_count = 4u,
            .safety_state = 4u,
            .fault_mask = 0x02u,
            .valid_mask = 0x77u,
            .control_rx_count = 0x1234u,
            .last_control_flags = 0x01u,
            .last_enabled_mask = 0x07u,
            .uptime_ms = 123456u,
            .stack_free_bytes = 512u,
        },
        .right = {
            .control_tx_count = 2000u,
            .telemetry_age_ms = UINT16_MAX,
        },
    };
    uint8_t frame[UGV_CRSF_DIAGNOSTIC_PAYLOAD_SIZE + 4u] = {0};
    const size_t size = ugv_crsf_build_diagnostic_frame(
        frame, sizeof(frame), &diagnostic);
    assert(size == UGV_CRSF_DIAGNOSTIC_PAYLOAD_SIZE + 4u);
    assert(frame[0] == 0xc8u);
    assert(frame[1] == UGV_CRSF_DIAGNOSTIC_PAYLOAD_SIZE + 2u);
    assert(frame[2] == UGV_CRSF_DIAGNOSTIC_FRAME_TYPE);
    assert(memcmp(&frame[3], "UGV\x03", 4u) == 0);
    assert(frame[7] == diagnostic.flags);
    assert(frame[8] == diagnostic.drive_mode);
    assert(frame[9] == 0x83u && frame[10] == 0xffu);  /* throttle -125, LE */
    assert(frame[11] == 0xfau && frame[12] == 0x00u); /* steering 250, LE */
    assert(frame[13] == 0x78u && frame[16] == 0x12u); /* channel_frame_count, LE */
    assert(frame[17] == 9u && frame[18] == 0u);       /* crc_error_count, LE */

    /* Left link-diagnostic block starts at frame[19]. */
    assert(frame[19] == 0xe8u && frame[20] == 0x03u); /* control_tx_count=1000 */
    assert(frame[23] == 2u && frame[24] == 0u);       /* control_tx_fail_count */
    assert(frame[25] == 0x5au && frame[26] == 0u);    /* telemetry_rx_count=90 */
    assert(frame[29] == 25u && frame[30] == 0u);      /* telemetry_age_ms */
    assert(frame[31] == 3u && frame[32] == 0u);       /* uart_crc_error_count */
    assert(frame[33] == 4u && frame[34] == 0u);       /* uart_format_error_count */
    assert(frame[35] == 4u);                          /* safety_state */
    assert(frame[36] == 0x02u);                       /* fault_mask */
    assert(frame[37] == 0x77u);                       /* valid_mask */
    assert(frame[38] == 0x34u);                       /* control_rx_count truncated to u8 */
    assert(frame[39] == 0x01u);                       /* last_control_flags */
    assert(frame[40] == 0x07u);                       /* last_enabled_mask */
    assert(frame[41] == 0x40u && frame[42] == 0xe2u &&
           frame[43] == 0x01u && frame[44] == 0u);    /* uptime_ms=123456, LE */
    assert(frame[45] == 0u && frame[46] == 2u);       /* stack_free_bytes=512, LE */

    /* Right link-diagnostic block starts at frame[47]; only two fields set,
     * the rest are zero-initialized. */
    assert(frame[47] == 0xd0u && frame[48] == 0x07u); /* control_tx_count=2000 */
    assert(frame[57] == 0xffu && frame[58] == 0xffu); /* telemetry_age_ms=never seen */

    assert(frame[sizeof(frame) - 1u] ==
           crc8_dvb_s2(&frame[2], UGV_CRSF_DIAGNOSTIC_PAYLOAD_SIZE + 1u));
    assert(ugv_crsf_build_diagnostic_frame(frame, sizeof(frame) - 1u, &diagnostic) == 0u);
}

int main(void)
{
    test_channels_and_normalization();
    test_link_statistics();
    test_rpm_telemetry_frame();
    test_diagnostic_frame();
    puts("all CRSF tests passed");
    return 0;
}
