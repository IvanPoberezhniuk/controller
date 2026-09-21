#include "app_main.h"

#include "board.h"
#include "configuration.h"
#include "current_monitor.h"
#include "encoder.h"
#include "fault_manager.h"
#include "motor_control.h"
#include "safety.h"
#include "timebase.h"
#include "uart_control_service.h"
#include "watchdog.h"

#ifndef UGV_BENCH_SELFTEST_SPIN_PERCENT
#define UGV_BENCH_SELFTEST_SPIN_PERCENT 0
#endif

void app_main_init(void)
{
    board_init();
    timebase_init();
    motor_control_init();
    encoder_init();
    current_monitor_init();
    fault_manager_init();
    safety_init();
    uart_control_service_init();

    /* A software bootloader jump preserves PRIMASK, unlike a hardware reset.
     * The bootloader restores it too, but the application defensively enables
     * global interrupts only after its state and UART RX ring are ready. */
    __enable_irq();
}

void app_main_uart2_rx_isr(void)
{
    uart_control_service_rx_isr();
}

#if UGV_BENCH_SELFTEST_SPIN_PERCENT
/* Bench-only diagnostic image: spin all three motors forward at a fixed
 * percent of max_target_rpm as soon as the control loop starts, bypassing
 * UART commands and the arm/safety state machine entirely. Built only under
 * a dedicated selftest CMake preset, flashed standalone over SWD -- never
 * part of the normal OTA image. Used to isolate PWM/enable/driver hardware
 * from the rest of the control stack while debugging "motors don't spin". */
static void selftest_run(float dt_s)
{
    const float target =
        config_get()->max_target_rpm * (UGV_BENCH_SELFTEST_SPIN_PERCENT / 100.0f);
    for (motor_index_t m = MOTOR_FRONT; m < UGV_MOTOR_COUNT; m++) {
        motor_control_set_enabled(m, true);
        (void)motor_control_set_target(m, target);
    }
    motor_control_step(dt_s);
    watchdog_refresh();
}
#endif

void app_main_run(void)
{
#if UGV_BENCH_SELFTEST_SPIN_PERCENT
    if (!timebase_tick_ready()) {
        return;
    }
    const float selftest_dt_s = timebase_dt_s();
    encoder_update(selftest_dt_s);
    current_monitor_sample();
    selftest_run(selftest_dt_s);
    return;
#endif

    uart_control_service_poll();

    if (!timebase_tick_ready()) {
        return;
    }

    const float dt_s = timebase_dt_s();
    encoder_update(dt_s);
    current_monitor_sample();
    fault_manager_update();
    safety_update();
    motor_control_step(dt_s);

    /* FAULT and EMERGENCY_STOP are healthy, intentional safety states. The
     * watchdog is refreshed after a complete control iteration and is starved
     * only if the loop actually stops making progress. */
    watchdog_refresh();

    static uint32_t telemetry_divider;
    if (++telemetry_divider >= (TIMEBASE_CONTROL_LOOP_HZ / 10u)) {
        telemetry_divider = 0u;
        uart_control_service_publish_telemetry();
    }
}
