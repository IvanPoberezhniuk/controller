#include "configuration.h"

/* Conservative bench-test defaults. None of these have been validated against
 * measured hardware behavior yet -- see ugv-project-plan SKILL.md's
 * unresolved-decisions register (final driver, encoder CPR, stall current).
 * Deliberately capped low (max_pwm, accel/decel) for first-power-on safety;
 * retune once motor0 is validated on the bench. */
static const ugv_config_t s_config = {
    .pid_kp                       = 0.02f,
    .pid_ki                       = 0.01f,
    .pid_kd                       = 0.0f,
    .integral_clamp               = 0.3f,
    .feedforward_gain             = 0.0f, /* PI-only until a feed-forward gain is measured */
    .pwm_dead_zone                = 0.05f,
    .pwm_min_effective            = 0.10f,
    .max_pwm                      = 0.5f,
    .accel_limit_pwm_per_s        = 1.0f,
    .decel_limit_pwm_per_s        = 2.0f,
    .direction_change_coast_ms    = 100u,
    .encoder_counts_per_output_rev = 44u, /* placeholder: 11 PPR x4 quadrature, UNVERIFIED */
    .command_timeout_ms           = 300u,
    .stall_min_duration_ms        = 500u,
    .stall_current_threshold_a    = 5.0f,
    .current_sense_scale_a_per_v  = 10.0f, /* UNVERIFIED placeholder */
};

const ugv_config_t *config_get(void)
{
    return &s_config;
}
