#include "app_main.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "timebase.h"
#include "watchdog.h"
#include "motor_control.h"
#include "encoder.h"
#include "current_monitor.h"
#include "fault_manager.h"
#include "safety.h"
#include "configuration.h"

/* Bring-up-only debug-UART command console. Stand-in for the CAN command
 * path (0x100/0x110 messages) until the "add CAN" roadmap milestone --
 * see shared/can/ugv_can_protocol.h. Commands: "arm", "estop", "clear",
 * "m<index> <rpm>" e.g. "m0 120". */
#define CMD_LINE_MAX 32
static char    s_line[CMD_LINE_MAX];
static uint8_t s_line_len;

static void process_command(const char *line)
{
    if (strcmp(line, "arm") == 0) {
        safety_request_arm();
        safety_notify_command_received();
    } else if (strcmp(line, "estop") == 0) {
        safety_request_emergency_stop();
    } else if (strcmp(line, "clear") == 0) {
        safety_clear_emergency_stop();
        safety_clear_fault();
    } else {
        int motor = -1;
        float rpm = 0.0f;
        if (sscanf(line, "m%d %f", &motor, &rpm) == 2 &&
            motor >= 0 && motor < (int)UGV_MOTOR_COUNT) {
            motor_control_set_target((motor_index_t)motor, rpm);
            safety_notify_command_received();
        }
    }
}

/* RX must be serviced from the USART2 interrupt, not polled from the main
 * loop: a control-loop tick can run for several ms (ADC sampling), which is
 * longer than a whole command line takes to arrive at 115200 baud -- and
 * once the USART's ORE flag latches, all further bytes are discarded until
 * it is cleared. The ISR captures bytes into this ring; the main loop only
 * parses. */
#define UART_RX_RING_SIZE 64u
static volatile uint8_t  s_rx_ring[UART_RX_RING_SIZE];
static volatile uint8_t  s_rx_ring_head;
static volatile uint8_t  s_rx_ring_tail;

void app_main_uart2_rx_isr(void)
{
    uint32_t isr = huart2.Instance->ISR;
    if (isr & UART_FLAG_RXNE) {
        uint8_t byte = (uint8_t)(huart2.Instance->RDR & 0xFFu);
        uint8_t next_head = (uint8_t)((s_rx_ring_head + 1u) % UART_RX_RING_SIZE);
        if (next_head != s_rx_ring_tail) {
            s_rx_ring[s_rx_ring_head] = byte;
            s_rx_ring_head = next_head;
        }
    }
    if (isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        __HAL_UART_CLEAR_NEFLAG(&huart2);
        __HAL_UART_CLEAR_FEFLAG(&huart2);
    }
}

static void poll_uart_rx(void)
{
    while (s_rx_ring_tail != s_rx_ring_head) {
        uint8_t byte = s_rx_ring[s_rx_ring_tail];
        s_rx_ring_tail = (uint8_t)((s_rx_ring_tail + 1u) % UART_RX_RING_SIZE);

        if (byte == '\r' || byte == '\n') {
            if (s_line_len > 0) {
                s_line[s_line_len] = '\0';
                process_command(s_line);
                s_line_len = 0;
            }
            continue;
        }

        if (s_line_len < (CMD_LINE_MAX - 1u)) {
            s_line[s_line_len++] = (char)byte;
        }
    }
}

static void print_telemetry(void)
{
    static uint32_t s_seq = 0;
    const MotorState *m0 = motor_control_get_state(MOTOR_FRONT);
    char buf[160];
    int len = snprintf(buf, sizeof(buf),
        "seq=%lu tick=%lu state=%s m0 target=%.1f meas=%.1f pwm=%.2f cur=%.2f enc_ok=%d stall=%d\r\n",
        (unsigned long)s_seq++, (unsigned long)HAL_GetTick(),
        safety_state_name(safety_get_state()),
        (double)m0->target_rpm, (double)m0->measured_rpm,
        (double)m0->pwm_command, (double)m0->current_a,
        m0->encoder_valid, m0->stalled);
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100u);
    }
}

void app_main_init(void)
{
    board_init();
    timebase_init();
    motor_control_init();
    encoder_init();
    current_monitor_init();
    fault_manager_init();
    safety_init();

    char buf[80];
    int len = snprintf(buf, sizeof(buf), "\r\nUGV %s node boot, reset=%s\r\n",
                        UGV_NODE_ROLE_NAME, board_reset_reason_name(board_get_reset_reason()));
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100u);
    }
}

void app_main_run(void)
{
    poll_uart_rx();

    if (!timebase_tick_ready()) {
        return;
    }

    float dt_s = timebase_dt_s();

    encoder_update(dt_s);
    current_monitor_sample();
    fault_manager_update();
    safety_update();
    motor_control_step(dt_s);

    /* FAULT and EMERGENCY_STOP are healthy, intentional safety states. The
     * watchdog is refreshed after a complete control iteration and is starved
     * only if the loop actually stops making progress. */
    watchdog_refresh();

    static uint32_t s_telemetry_divider = 0;
    if (++s_telemetry_divider >= (TIMEBASE_CONTROL_LOOP_HZ / 4u)) {
        s_telemetry_divider = 0;
        print_telemetry();
    }
}
