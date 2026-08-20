#include "current_monitor.h"
#include "motor_control.h"
#include "configuration.h"
#include "board.h"

/* VDDA/VREF+ are tied together on this design (no precision reference) --
 * see the ADC pin diagram from the CubeMX session. */
#define ADC_VREF_V     3.3f
#define ADC_MAX_COUNT  4095.0f
#define ADC_POLL_TIMEOUT_MS 1u

/* All motor current-sense lines go through a CD74HC4067 16-channel analog
 * mux into a single ADC2 input (MUX_SIG) instead of one ADC pin per signal.
 * This replaced the old direct-wired layout that split across ADC1/ADC2 and
 * had two ranks (CH13/CH17, motor1 L_IS/R_IS) that never raised EOC -- see
 * memory [[fdcan-pin-conflict]]. Channels 6-15 are reserved on the mux for
 * future motor-temperature/supply-voltage sensing but not sampled yet: no
 * temperature sensor is chosen (MotorState.temperature_c stays unwired). */
/* CD74HC4067 channel-select settle time: on-resistance (~100 ohm typical)
 * charging the ADC sample-and-hold plus stray capacitance on the shared SIG
 * trace. A few microseconds of margin costs nothing against the 2 ms
 * control-loop budget -- revisit if bench readings look noisy right after a
 * channel change. */
#define MUX_SETTLE_US 5u

#if UGV_MUX_GPIO_CONFIGURED

static float counts_to_amps(uint32_t raw_counts)
{
    float voltage = ((float)raw_counts / ADC_MAX_COUNT) * ADC_VREF_V;
    return voltage * config_get()->current_sense_scale_a_per_v;
}

static void mux_select(uint8_t channel)
{
    HAL_GPIO_WritePin(MUX_S0_GPIO_Port, MUX_S0_Pin, (channel & 0x01u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_S1_GPIO_Port, MUX_S1_Pin, (channel & 0x02u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_S2_GPIO_Port, MUX_S2_Pin, (channel & 0x04u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_S3_GPIO_Port, MUX_S3_Pin, (channel & 0x08u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    board_delay_us(MUX_SETTLE_US);
}

static uint32_t mux_read_raw(uint8_t channel)
{
    mux_select(channel);

    uint32_t raw = 0u;
    if (HAL_ADC_Start(&hadc2) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc2, ADC_POLL_TIMEOUT_MS) == HAL_OK) {
            raw = HAL_ADC_GetValue(&hadc2);
        }
        HAL_ADC_Stop(&hadc2);
    }
    /* Restarting a conversion every call (rather than running continuously)
     * can latch ADC_FLAG_OVR (overrun) even when the value was read out in
     * time -- once set, OVR stays latched forever and was observed (via
     * SWD) to stall subsequent HAL_ADC_PollForConversion() calls for
     * seconds at a time. Must be cleared explicitly; HAL does not do this
     * in polling mode. */
    __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_OVR);
    return raw;
}

void current_monitor_init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
}

void current_monitor_sample(void)
{
    float ris_a[UGV_MOTOR_COUNT];
    float lis_a[UGV_MOTOR_COUNT];

    ris_a[MOTOR_FRONT]  = counts_to_amps(mux_read_raw(UGV_MUX_CH_MOTOR0_RIS));
    lis_a[MOTOR_FRONT]  = counts_to_amps(mux_read_raw(UGV_MUX_CH_MOTOR0_LIS));
    ris_a[MOTOR_CENTER] = counts_to_amps(mux_read_raw(UGV_MUX_CH_MOTOR1_RIS));
    lis_a[MOTOR_CENTER] = counts_to_amps(mux_read_raw(UGV_MUX_CH_MOTOR1_LIS));
    ris_a[MOTOR_REAR]   = counts_to_amps(mux_read_raw(UGV_MUX_CH_MOTOR2_RIS));
    lis_a[MOTOR_REAR]   = counts_to_amps(mux_read_raw(UGV_MUX_CH_MOTOR2_LIS));

    /* Whichever half-bridge is actively driving is the meaningful reading;
     * the other side reads ~0. */
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        motor_control_get_state_mutable(m)->current_a =
            (ris_a[m] > lis_a[m]) ? ris_a[m] : lis_a[m];
    }
}

#else

/* Safe bring-up fallback. The channel map is defined, but CubeMX does not yet
 * assign MUX S0-S3 and SIG pins, so current data must remain unavailable
 * instead of binding the mux to guessed GPIOs. */
void current_monitor_init(void)
{
}

void current_monitor_sample(void)
{
}

#endif /* UGV_MUX_GPIO_CONFIGURED */
