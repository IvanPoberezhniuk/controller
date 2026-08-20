#include "current_monitor.h"
#include "motor_control.h"
#include "configuration.h"
#include "board.h"

/* VDDA/VREF+ are tied together on this design (no precision reference) --
 * see the ADC pin diagram from the CubeMX session. */
#define ADC_VREF_V     3.3f
#define ADC_MAX_COUNT  4095.0f
#define ADC_POLL_TIMEOUT_MS 1u

/* The intended hardware routes all current-sense lines through one local
 * CD74HC4067 into a single MUX_SIG ADC input. Channels 0-5 are R_IS/L_IS;
 * channels 6-8 are reserved for the three motor temperatures. Temperature
 * acquisition is not implemented until the sensor type is selected. */
/* CD74HC4067 channel-select settle time: on-resistance (~100 ohm typical)
 * charging the ADC sample-and-hold plus stray capacitance on the shared SIG
 * trace. A few microseconds of margin costs nothing against the 2 ms
 * control-loop budget -- revisit if bench readings look noisy right after a
 * channel change. */
#define MUX_SETTLE_US 5u

#if UGV_MUX_GPIO_CONFIGURED

static bool s_adc_ready;

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

static bool mux_read_raw(uint8_t channel, uint32_t *raw)
{
    mux_select(channel);

    bool valid = false;
    if (HAL_ADC_Start(&hadc2) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc2, ADC_POLL_TIMEOUT_MS) == HAL_OK) {
            *raw = HAL_ADC_GetValue(&hadc2);
            valid = true;
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
    return valid;
}

void current_monitor_init(void)
{
    s_adc_ready =
        (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) == HAL_OK);
}

void current_monitor_sample(void)
{
    static const uint8_t ris_channel[UGV_MOTOR_COUNT] = {
        UGV_MUX_CH_MOTOR0_RIS,
        UGV_MUX_CH_MOTOR1_RIS,
        UGV_MUX_CH_MOTOR2_RIS,
    };
    static const uint8_t lis_channel[UGV_MOTOR_COUNT] = {
        UGV_MUX_CH_MOTOR0_LIS,
        UGV_MUX_CH_MOTOR1_LIS,
        UGV_MUX_CH_MOTOR2_LIS,
    };

    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        MotorState *state = motor_control_get_state_mutable(m);
        uint32_t ris_raw = 0u;
        uint32_t lis_raw = 0u;
        bool valid = s_adc_ready &&
            mux_read_raw(ris_channel[m], &ris_raw) &&
            mux_read_raw(lis_channel[m], &lis_raw);

        state->current_valid = valid;
        if (valid) {
            float ris_a = counts_to_amps(ris_raw);
            float lis_a = counts_to_amps(lis_raw);
            state->current_a = (ris_a > lis_a) ? ris_a : lis_a;
        } else {
            state->current_a = 0.0f;
        }
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
    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        MotorState *state = motor_control_get_state_mutable(m);
        state->current_a = 0.0f;
        state->current_valid = false;
    }
}

#endif /* UGV_MUX_GPIO_CONFIGURED */
