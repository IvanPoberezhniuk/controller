#include "ugv_manual_control.h"

#include <string.h>

#define RC_DEADBAND             0.05f
#define RC_ARM_LOW_THRESHOLD   (-0.50f)
#define RC_ARM_HIGH_THRESHOLD   0.50f
#define RC_ARM_NEUTRAL_LIMIT    0.08f

#define UGV_MANUAL_CONTROL_WHEEL_COUNT 3u

static void mix_drive(ugv_manual_control_t *control)
{
    /* Match connectionApp's vehicle convention: positive steering accelerates
     * the right side and slows/reverses the left side. */
    float left = control->throttle - (control->steering * 0.5f);
    float right = control->throttle + (control->steering * 0.5f);
    float largest = left < 0.0f ? -left : left;
    const float right_magnitude = right < 0.0f ? -right : right;
    if (right_magnitude > largest) {
        largest = right_magnitude;
    }
    if (largest > 1.0f) {
        left /= largest;
        right /= largest;
    }
    const int16_t left_rpm = (int16_t)(left * UGV_RC_MAX_RPM);
    const int16_t right_rpm = (int16_t)(right * UGV_RC_MAX_RPM);

    control->left_enable_mask = UGV_WHEEL_ENABLE_REAR;
    if (control->drive_mode >= UGV_DRIVE_MODE_4WD) {
        control->left_enable_mask |= UGV_WHEEL_ENABLE_CENTER;
    }
    if (control->drive_mode >= UGV_DRIVE_MODE_6WD) {
        control->left_enable_mask |= UGV_WHEEL_ENABLE_FRONT;
    }
    control->right_enable_mask = control->left_enable_mask;

    for (unsigned wheel = 0; wheel < UGV_MANUAL_CONTROL_WHEEL_COUNT; ++wheel) {
        const bool enabled = (control->left_enable_mask & (1u << wheel)) != 0u;
        control->left_rpm[wheel] = enabled ? left_rpm : 0;
        control->right_rpm[wheel] = enabled ? right_rpm : 0;
    }
}

static void stop_all_wheels(ugv_manual_control_t *control)
{
    control->left_enable_mask = 0u;
    control->right_enable_mask = 0u;
    memset(control->left_rpm, 0, sizeof(control->left_rpm));
    memset(control->right_rpm, 0, sizeof(control->right_rpm));
}

void ugv_manual_control_init(ugv_manual_control_t *control)
{
    if (control != NULL) {
        memset(control, 0, sizeof(*control));
    }
}

void ugv_manual_control_note_channels(ugv_manual_control_t *control,
                                      uint32_t now_ms)
{
    if (control != NULL) {
        control->last_channels_ms = now_ms;
    }
}

void ugv_manual_control_note_link_stats(ugv_manual_control_t *control,
                                        uint32_t now_ms)
{
    if (control != NULL) {
        control->last_link_stats_ms = now_ms;
    }
}

void ugv_manual_control_update(ugv_manual_control_t *control,
                               const ugv_crsf_receiver_t *radio,
                               uint32_t now_ms)
{
    if (control == NULL || radio == NULL) {
        return;
    }

    const bool channels_fresh = radio->channel_frame_count > 0u &&
        (now_ms - control->last_channels_ms) <= UGV_RC_LINK_TIMEOUT_MS;
    const bool stats_fresh = radio->link_stats_seen &&
        (now_ms - control->last_link_stats_ms) <= UGV_RC_LINK_TIMEOUT_MS;
    const bool receiver_reports_link = !stats_fresh || radio->link_quality_pct > 0u;
    const bool link_up = channels_fresh && receiver_reports_link;

    if (!link_up) {
        control->link_up = false;
        control->armed = false;
        control->arm_low_seen = false;
        control->previous_arm_high = false;
        control->steering = 0.0f;
        control->throttle = 0.0f;
        stop_all_wheels(control);
        return;
    }

    control->link_up = true;
    control->steering = ugv_crsf_channel_normalized(
        radio, UGV_RC_STEERING_CHANNEL, RC_DEADBAND);
    control->throttle = ugv_crsf_channel_normalized(
        radio, UGV_RC_THROTTLE_CHANNEL, RC_DEADBAND);
    const float drive_mode = ugv_crsf_channel_normalized(
        radio, UGV_RC_DRIVE_MODE_CHANNEL, 0.0f);
    control->drive_mode = drive_mode < -0.5f ? UGV_DRIVE_MODE_2WD :
                          drive_mode > 0.5f ? UGV_DRIVE_MODE_6WD : UGV_DRIVE_MODE_4WD;

    const float arm = ugv_crsf_channel_normalized(
        radio, UGV_RC_ARM_CHANNEL, 0.0f);
    const bool arm_low = arm < RC_ARM_LOW_THRESHOLD;
    const bool arm_high = arm > RC_ARM_HIGH_THRESHOLD;
    const float estop = ugv_crsf_channel_normalized(
        radio, UGV_RC_ESTOP_CHANNEL, 0.0f);
    const bool estop_high = estop > RC_ARM_HIGH_THRESHOLD;

    if (estop_high) {
        control->emergency_stop_latched = true;
        control->armed = false;
    }

    if (arm_low) {
        control->arm_low_seen = true;
        control->armed = false;
    }

    const float steering_magnitude = control->steering < 0.0f
                                         ? -control->steering
                                         : control->steering;
    const float throttle_magnitude = control->throttle < 0.0f
                                         ? -control->throttle
                                         : control->throttle;
    const bool sticks_neutral = steering_magnitude <= RC_ARM_NEUTRAL_LIMIT &&
                                throttle_magnitude <= RC_ARM_NEUTRAL_LIMIT;

    if (arm_high && !control->previous_arm_high &&
        control->arm_low_seen && sticks_neutral && !estop_high) {
        control->emergency_stop_latched = false;
        control->armed = true;
    } else if (!arm_high) {
        control->armed = false;
    }
    control->previous_arm_high = arm_high;

    if (control->armed && !control->emergency_stop_latched) {
        mix_drive(control);
    } else {
        stop_all_wheels(control);
    }
}
