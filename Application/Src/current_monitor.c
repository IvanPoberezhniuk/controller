#include "current_monitor.h"
#include "motor_control.h"
#include "configuration.h"
#include "board.h"

/* VDDA/VREF+ are tied together on this design (no precision reference) --
 * see the ADC pin diagram from the CubeMX session. */
#define ADC_VREF_V     3.3f
#define ADC_MAX_COUNT  4095.0f
/* 1 ms, not 10 ms: ADC2 ranks 4/5 (CH13/CH17, motor1 L_IS/R_IS on PA5/PA4)
 * fail HAL_ADC_PollForConversion() on every tick regardless of clock
 * source/speed (unresolved, see comment below), so a long timeout on them
 * only stalls the control loop. Still ample headroom for the channels that
 * work -- real conversions complete in microseconds. */
#define ADC_POLL_TIMEOUT_MS 1u

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
    /* Restarting a scan sequence every tick (rather than running it
     * continuously) can latch ADC_FLAG_OVR (overrun) even when every value
     * here was actually read out in time -- once set, OVR stays latched
     * forever and was observed (via SWD) to stall subsequent
     * HAL_ADC_PollForConversion() calls for seconds at a time. Must be
     * cleared explicitly; HAL does not do this in polling mode. */
    __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_OVR);

    uint32_t adc1_raw = 0u;
    if (HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1, ADC_POLL_TIMEOUT_MS) == HAL_OK) {
            adc1_raw = HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_OVR);

    /* Rank order from MX_ADC2_Init: CH3, CH4, CH12, CH13, CH17. KNOWN
     * LIMITATION: ranks 4/5 (CH13/CH17, motor1 L_IS/R_IS on PA5/PA4) fail
     * conversion on every tick while ranks 1-3 and ADC1 never do, so
     * motor1_lis_a/ris_a are always 0. Unresolved -- possibly an internal
     * OPAMP/buffer routing quirk of these channels on this package. */
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
