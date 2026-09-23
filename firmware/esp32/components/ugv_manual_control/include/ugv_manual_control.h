#ifndef UGV_MANUAL_CONTROL_H
#define UGV_MANUAL_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "ugv_crsf.h"
#include "ugv_uart_protocol.h"

enum {
    UGV_RC_STEERING_CHANNEL = 0, /* EdgeTX CH1 / Aileron */
    UGV_RC_THROTTLE_CHANNEL = 1, /* EdgeTX CH2 / Elevator */
    UGV_RC_DRIVE_MODE_CHANNEL = 2, /* CH3: 2WD / 4WD / 6WD */
    UGV_RC_ARM_CHANNEL = 4,      /* EdgeTX CH5 / two-position switch */
    UGV_RC_ESTOP_CHANNEL = 5,    /* EdgeTX CH6 / emergency stop */
    UGV_RC_CLEAR_FAULT_CHANNEL = 6, /* EdgeTX CH7 / clears latched STM32 FAULT */
};

#define UGV_RC_MAX_RPM 200.0f

typedef enum {
    UGV_DRIVE_MODE_2WD = 1u,
    UGV_DRIVE_MODE_4WD = 2u,
    UGV_DRIVE_MODE_6WD = 3u,
} ugv_drive_mode_t;

typedef struct {
    bool link_up;
    bool armed;
    bool arm_low_seen;
    bool previous_arm_high;
    bool emergency_stop_latched;
    bool clear_fault_requested;
    uint32_t last_channels_ms;
    uint32_t last_link_stats_ms;
    float steering;
    float throttle;
    uint8_t drive_mode;
    uint8_t left_enable_mask;
    uint8_t right_enable_mask;
    int16_t left_rpm[3];
    int16_t right_rpm[3];
} ugv_manual_control_t;

void ugv_manual_control_init(ugv_manual_control_t *control);
void ugv_manual_control_note_channels(ugv_manual_control_t *control,
                                      uint32_t now_ms);
void ugv_manual_control_note_link_stats(ugv_manual_control_t *control,
                                        uint32_t now_ms);
void ugv_manual_control_update(ugv_manual_control_t *control,
                               const ugv_crsf_receiver_t *radio,
                               uint32_t now_ms);

#endif /* UGV_MANUAL_CONTROL_H */
