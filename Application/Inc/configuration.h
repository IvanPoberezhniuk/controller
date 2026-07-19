#ifndef APPLICATION_CONFIGURATION_H
#define APPLICATION_CONFIGURATION_H

#include <stdint.h>

/* Node role is selected at build time (-DUGV_NODE_ROLE_LEFT or
 * -DUGV_NODE_ROLE_RIGHT via the CMake UGV_NODE_ROLE option) since both
 * nodes run identical source. */
#if defined(UGV_NODE_ROLE_LEFT)
#define UGV_NODE_ROLE_NAME "left"
#elif defined(UGV_NODE_ROLE_RIGHT)
#define UGV_NODE_ROLE_NAME "right"
#else
#error "UGV_NODE_ROLE must be defined to LEFT or RIGHT (see CMakeLists.txt UGV_NODE_ROLE option)"
#endif

#define UGV_MOTOR_COUNT 3

/* Tunables, per ugv-telemetry-safety SKILL.md "Configuration management".
 * Compiled-in defaults; not yet backed by persistent storage/versioning --
 * that comes with the "add CAN" / telemetry milestone. */
typedef struct {
    float    pid_kp;
    float    pid_ki;
    float    pid_kd;               /* 0 until measurement noise is characterized */
    float    integral_clamp;
    float    feedforward_gain;     /* PWM fraction per target RPM; 0 until measured */
    float    pwm_dead_zone;        /* 0..1 fraction of max PWM */
    float    pwm_min_effective;    /* 0..1, calibrated minimum to overcome static friction */
    float    max_pwm;              /* 0..1 fraction of TIM ARR */
    float    accel_limit_rpm_per_s; /* ramps target_rpm, per ugv-stm32-firmware
                                      * SKILL.md's "acceleration limiter -> per-wheel
                                      * target RPM" command-path stage -- RPM units,
                                      * not a PWM fraction */
    float    decel_limit_rpm_per_s;
    uint32_t direction_change_coast_ms;
    uint32_t encoder_counts_per_output_rev; /* TODO: measure per calibration procedure in
                                              * ugv-motor-driver-encoders SKILL.md -- do not
                                              * assume from nominal "11 PPR" datasheet figure */
    uint32_t command_timeout_ms;
    uint32_t stall_min_duration_ms;
    float    stall_current_threshold_a;     /* TODO: measure, see ugv-drivetrain */
    float    current_sense_scale_a_per_v;   /* TODO: calibrate against the actual R_IS/L_IS
                                              * sense resistor value once the BTS7960 board
                                              * is populated -- see ugv-motor-driver-encoders */
} ugv_config_t;

const ugv_config_t *config_get(void);

#endif /* APPLICATION_CONFIGURATION_H */
