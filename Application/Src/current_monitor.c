#include "current_monitor.h"
#include "motor_control.h"
#include "configuration.h"
#include "board.h"

/* VDDA/VREF+ are tied together on this design (no precision reference) --
 * see the ADC pin diagram from the CubeMX session. */
#define ADC_VREF_V     3.3f
#define ADC_MAX_COUNT  4095.0f
#define ADC_POLL_TIMEOUT_MS 10u

static float counts_to_amps(uint32_t raw_counts)
{
    float voltage = ((float)raw_counts / ADC_MAX_COUNT) * ADC_VREF_V;
    return voltage * config_get()->current_sense_scale_a_per_v;
}

void current_monitor_init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
}

void current_monitor_sample(void)
{
    uint32_t adc2_raw[5] = {0};

    if (HAL_ADC_Start(&hadc2) == HAL_OK) {
        for (int i = 0; i < 5; i++) {
            if (HAL_ADC_PollForConversion(&hadc2, ADC_POLL_TIMEOUT_MS) == HAL_OK) {
                adc2_raw[i] = HAL_ADC_GetValue(&hadc2);
            }
        }
        HAL_ADC_Stop(&hadc2);
    }

    uint32_t adc1_raw = 0u;
    if (HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1, ADC_POLL_TIMEOUT_MS) == HAL_OK) {
            adc1_raw = HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }

    /* Rank order from MX_ADC2_Init: CH3, CH4, CH12, CH13, CH17. */
    float motor0_ris_a = counts_to_amps(adc2_raw[0]);
    float motor0_lis_a = counts_to_amps(adc2_raw[1]);
    float motor2_ris_a = counts_to_amps(adc2_raw[2]);
    float motor1_lis_a = counts_to_amps(adc2_raw[3]);
    float motor1_ris_a = counts_to_amps(adc2_raw[4]);
    float motor2_lis_a = counts_to_amps(adc1_raw);

    float motor0_a = (motor0_ris_a > motor0_lis_a) ? motor0_ris_a : motor0_lis_a;
    float motor1_a = (motor1_ris_a > motor1_lis_a) ? motor1_ris_a : motor1_lis_a;
    float motor2_a = (motor2_ris_a > motor2_lis_a) ? motor2_ris_a : motor2_lis_a;

    motor_control_get_state_mutable(MOTOR_FRONT)->current_a  = motor0_a;
    motor_control_get_state_mutable(MOTOR_CENTER)->current_a = motor1_a;
    motor_control_get_state_mutable(MOTOR_REAR)->current_a   = motor2_a;
}
