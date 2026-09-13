#include <assert.h>
#include <stdio.h>

#include "ugv_manual_control.h"

#define CRSF_MIN    172u
#define CRSF_CENTER 992u
#define CRSF_MAX    1811u

static void set_channel(ugv_crsf_receiver_t *radio, unsigned channel,
                        uint16_t value)
{
    radio->channels[channel] = value;
}

static void fresh_update(ugv_manual_control_t *control,
                         ugv_crsf_receiver_t *radio,
                         uint32_t now_ms)
{
    radio->channel_frame_count++;
    ugv_manual_control_note_channels(control, now_ms);
    ugv_manual_control_update(control, radio, now_ms);
}

static void test_explicit_neutral_arm_and_mixer(void)
{
    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);

    /* Starting with ARM high must never auto-arm. */
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 0u);
    assert(control.link_up);
    assert(!control.armed);

    /* A low-to-high transition with neutral sticks explicitly arms. */
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 1u);
    assert(control.arm_low_seen);
    assert(!control.armed);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 2u);
    assert(control.armed);

    /* connectionApp maps positive throttle to the high CRSF endpoint. */
    set_channel(&radio, UGV_RC_DRIVE_MODE_CHANNEL, CRSF_MAX);
    set_channel(&radio, UGV_RC_THROTTLE_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 3u);
    assert(control.drive_mode == 3u);
    assert(control.left_enable_mask == UGV_CAN_WHEEL_ENABLE_ALL);
    assert(control.right_enable_mask == UGV_CAN_WHEEL_ENABLE_ALL);
    for (unsigned wheel = 0; wheel < 3u; ++wheel) {
        assert(control.left_rpm[wheel] == 200);
        assert(control.right_rpm[wheel] == 200);
    }

    /* Zero throttle plus full right steering performs a point turn. */
    set_channel(&radio, UGV_RC_THROTTLE_CHANNEL, CRSF_CENTER);
    set_channel(&radio, UGV_RC_STEERING_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 4u);
    for (unsigned wheel = 0; wheel < 3u; ++wheel) {
        assert(control.left_rpm[wheel] == -100);
        assert(control.right_rpm[wheel] == 100);
    }
}

static void test_drive_modes_control_each_wheel(void)
{
    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);

    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 0u);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 1u);
    set_channel(&radio, UGV_RC_THROTTLE_CHANNEL, CRSF_MAX);

    set_channel(&radio, UGV_RC_DRIVE_MODE_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 2u);
    assert(control.drive_mode == 1u);
    assert(control.left_enable_mask == UGV_CAN_WHEEL_ENABLE_REAR);
    assert(control.right_enable_mask == UGV_CAN_WHEEL_ENABLE_REAR);
    assert(control.left_rpm[0] == 0 && control.left_rpm[1] == 0 &&
           control.left_rpm[2] == 200);

    set_channel(&radio, UGV_RC_DRIVE_MODE_CHANNEL, CRSF_CENTER);
    fresh_update(&control, &radio, 3u);
    assert(control.drive_mode == 2u);
    assert(control.left_enable_mask ==
           (UGV_CAN_WHEEL_ENABLE_CENTER | UGV_CAN_WHEEL_ENABLE_REAR));
    assert(control.right_enable_mask ==
           (UGV_CAN_WHEEL_ENABLE_CENTER | UGV_CAN_WHEEL_ENABLE_REAR));
    assert(control.right_rpm[0] == 0 && control.right_rpm[1] == 200 &&
           control.right_rpm[2] == 200);

    set_channel(&radio, UGV_RC_DRIVE_MODE_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 4u);
    assert(control.drive_mode == 3u);
    assert(control.left_enable_mask == UGV_CAN_WHEEL_ENABLE_ALL);
    assert(control.right_enable_mask == UGV_CAN_WHEEL_ENABLE_ALL);
}

static void test_failsafe_requires_rearm(void)
{
    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);

    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 10u);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 11u);
    assert(control.armed);

    ugv_manual_control_update(&control, &radio, 112u);
    assert(!control.link_up);
    assert(!control.armed);
    assert(control.left_enable_mask == 0u);
    assert(control.right_enable_mask == 0u);
    for (unsigned wheel = 0; wheel < 3u; ++wheel) {
        assert(control.left_rpm[wheel] == 0);
        assert(control.right_rpm[wheel] == 0);
    }
    assert(!control.arm_low_seen);

    /* Link returning while ARM remains high is not enough. */
    fresh_update(&control, &radio, 113u);
    assert(control.link_up);
    assert(!control.armed);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 114u);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 115u);
    assert(control.armed);
}

static void test_non_neutral_arm_is_rejected(void)
{
    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);

    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 0u);
    set_channel(&radio, UGV_RC_THROTTLE_CHANNEL, CRSF_MIN);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 1u);
    assert(!control.armed);

    /* Returning the stick to neutral while ARM stays high still cannot arm. */
    set_channel(&radio, UGV_RC_THROTTLE_CHANNEL, CRSF_CENTER);
    fresh_update(&control, &radio, 2u);
    assert(!control.armed);
}

static void test_estop_latches_until_explicit_rearm(void)
{
    ugv_crsf_receiver_t radio;
    ugv_crsf_init(&radio);
    ugv_manual_control_t control;
    ugv_manual_control_init(&control);

    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 0u);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 1u);
    assert(control.armed);

    set_channel(&radio, UGV_RC_ESTOP_CHANNEL, CRSF_MAX);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 2u);
    assert(control.emergency_stop_latched);
    assert(!control.armed);

    set_channel(&radio, UGV_RC_ESTOP_CHANNEL, CRSF_MIN);
    fresh_update(&control, &radio, 3u);
    assert(control.emergency_stop_latched);
    set_channel(&radio, UGV_RC_ARM_CHANNEL, CRSF_MAX);
    fresh_update(&control, &radio, 4u);
    assert(!control.emergency_stop_latched);
    assert(control.armed);
}

int main(void)
{
    test_explicit_neutral_arm_and_mixer();
    test_drive_modes_control_each_wheel();
    test_failsafe_requires_rearm();
    test_non_neutral_arm_is_rejected();
    test_estop_latches_until_explicit_rearm();
    puts("all manual-control tests passed");
    return 0;
}
