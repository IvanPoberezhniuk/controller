#include "current_monitor.h"
#include "motor_control.h"
#include "configuration.h"
#include "board.h"

/* VDDA/VREF+ are tied together on this design (no precision reference) --
 * see the ADC pin diagram from the CubeMX session. */
#define ADC_VREF_V     3.3f
#define ADC_MAX_COUNT  4095.0f
#define ADC_POLL_TIMEOUT_MS 1u
#define ADC2_DIRECT_CHANNEL_COUNT 5u
#define ADC1_DIRECT_CHANNEL_COUNT 1u

/* Direct ADC2 scan order configured by CubeMX:
 * rank 1 PA6/front R_IS, rank 2 PA7/front L_IS,
 * rank 3 PB2/rear R_IS, rank 4 PA5/center L_IS,
 * rank 5 PA4/center R_IS. ADC1 rank 1 is PB12/rear L_IS. */
enum {
    ADC2_FRONT_RIS_RANK = 0,
    ADC2_FRONT_LIS_RANK,
    ADC2_REAR_RIS_RANK,
    ADC2_CENTER_LIS_RANK,
    ADC2_CENTER_RIS_RANK,
};

static bool s_adc_ready;

static float counts_to_amps(uint32_t raw_counts)
{
    float voltage = ((float)raw_counts / ADC_MAX_COUNT) * ADC_VREF_V;
    return voltage * config_get()->current_sense_scale_a_per_v;
}

static bool adc_read_scan(ADC_HandleTypeDef *adc, uint32_t *raw,
                          uint32_t conversion_count)
{
    if (adc == NULL || raw == NULL || conversion_count == 0u) {
        return false;
    }

    __HAL_ADC_CLEAR_FLAG(adc, ADC_FLAG_OVR);
    if (HAL_ADC_Start(adc) != HAL_OK) {
        return false;
    }

    bool valid = true;
    for (uint32_t i = 0u; i < conversion_count; i++) {
        if (HAL_ADC_PollForConversion(adc, ADC_POLL_TIMEOUT_MS) != HAL_OK) {
            valid = false;
            break;
        }
        raw[i] = HAL_ADC_GetValue(adc);
    }

    if (HAL_ADC_Stop(adc) != HAL_OK) {
        valid = false;
    }
    __HAL_ADC_CLEAR_FLAG(adc, ADC_FLAG_OVR);
    return valid;
}

void current_monitor_init(void)
{
    bool adc1_ready =
        (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK);
    bool adc2_ready =
        (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) == HAL_OK);

    s_adc_ready = adc1_ready && adc2_ready &&
        hadc1.Init.NbrOfConversion == ADC1_DIRECT_CHANNEL_COUNT &&
        hadc2.Init.NbrOfConversion == ADC2_DIRECT_CHANNEL_COUNT;
}

void current_monitor_sample(void)
{
    uint32_t adc2_raw[ADC2_DIRECT_CHANNEL_COUNT] = {0u};
    uint32_t adc1_raw[ADC1_DIRECT_CHANNEL_COUNT] = {0u};
    bool valid = s_adc_ready &&
        adc_read_scan(&hadc2, adc2_raw, ADC2_DIRECT_CHANNEL_COUNT) &&
        adc_read_scan(&hadc1, adc1_raw, ADC1_DIRECT_CHANNEL_COUNT);

    const uint32_t ris_raw[UGV_MOTOR_COUNT] = {
        [MOTOR_FRONT] = adc2_raw[ADC2_FRONT_RIS_RANK],
        [MOTOR_CENTER] = adc2_raw[ADC2_CENTER_RIS_RANK],
        [MOTOR_REAR] = adc2_raw[ADC2_REAR_RIS_RANK],
    };
    const uint32_t lis_raw[UGV_MOTOR_COUNT] = {
        [MOTOR_FRONT] = adc2_raw[ADC2_FRONT_LIS_RANK],
        [MOTOR_CENTER] = adc2_raw[ADC2_CENTER_LIS_RANK],
        [MOTOR_REAR] = adc1_raw[0],
    };

    for (motor_index_t m = 0; m < UGV_MOTOR_COUNT; m++) {
        MotorState *state = motor_control_get_state_mutable(m);
        state->current_valid = valid;
        if (valid) {
            float ris_a = counts_to_amps(ris_raw[m]);
            float lis_a = counts_to_amps(lis_raw[m]);
            state->current_a = (ris_a > lis_a) ? ris_a : lis_a;
        } else {
            state->current_a = 0.0f;
        }
    }
}
