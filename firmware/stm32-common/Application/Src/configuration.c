#include "configuration.h"

/* Bench-test values. PID/feedforward and the encoder scale are tuned against
 * measured motor0 behavior; current sensing remains uncalibrated. Accel/decel
 * limits remain bench-conservative. */
static const ugv_config_t s_config = {
    .pid_kp                       = 0.002f, /* plant gain ~333 RPM per unit PWM (datasheet
                                              * no-load at 12 V, confirmed on the bench);
                                              * the original kp=0.02 gave loop gain >>1 and
                                              * a hard bang-bang limit cycle */
    .pid_ki                       = 0.01f,
    .pid_kd                       = 0.0f,
    .integral_clamp               = 30.0f, /* clamp is in integral units (RPM*s): max
                                             * integral authority = ki * clamp = 0.3 PWM.
                                             * The old 0.3 limited the I-term to 0.003 PWM
                                             * -- effectively disabled, leaving permanent
                                             * steady-state error */
    .feedforward_gain             = 0.003f, /* 1/333 -- nominal duty per RPM; PID only
                                              * trims the residual */
    .pwm_dead_zone                = 0.05f,
    .pwm_min_effective            = 0.10f,
    .max_pwm                      = 1.0f, /* full authority -- motor0 speed loop
                                             * validated on the bench at 10/100/250 RPM */
    .max_target_rpm               = 333.0f,
    .accel_limit_rpm_per_s        = 100.0f, /* bench-conservative: 0->60 RPM in ~0.6s */
    .decel_limit_rpm_per_s        = 200.0f,
    .direction_change_coast_ms    = 100u,
    .encoder_counts_per_output_rev = 2640u, /* MEASURED on the bench (3 duty/speed points
                                              * against the datasheet 333 RPM no-load
                                              * curve), not the naive 11PPRx4x30=1320 from
                                              * the datasheets -- the 22-pole magnetic ring
                                              * yields 88 counts per motor rev in TI12
                                              * mode, twice the nominal "11 PPR" figure */
    .command_timeout_ms           = UGV_CAN_COMMAND_TIMEOUT_MS,
    .stall_min_duration_ms        = 500u,
    .stall_current_threshold_a    = 50.0f, /* temporarily raised well above the observed
                                             * ADC noise floor (~0-5A-equivalent) on the
                                             * still-unwired/uncalibrated R_IS/L_IS current
                                             * sense -- was spuriously tripping overcurrent
                                             * and flickering the enable pins. Lower back
                                             * down once current sensing is actually wired
                                             * and calibrated. */
    .current_sense_scale_a_per_v  = 10.0f, /* UNVERIFIED placeholder */
};

const ugv_config_t *config_get(void)
{
    return &s_config;
}
