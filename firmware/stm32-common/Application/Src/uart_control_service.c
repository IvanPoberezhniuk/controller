#include "uart_control_service.h"

#include <limits.h>
#include <stdint.h>

#include "board.h"
#include "configuration.h"
#include "motor_control.h"
#include "safety.h"
#include "stack_watermark.h"
#ifdef UGV_OTA_APP
#include "ugv_boot_request_stm32.h"
#include "ugv_fw_update_protocol.h"
#endif
#include "ugv_uart_protocol.h"

#ifdef UGV_OTA_APP
#ifndef UGV_BOOT_NODE_ID
#error "UGV_BOOT_NODE_ID must identify the LEFT or RIGHT motor node"
#endif
#endif

#define UART_RX_RING_SIZE 128u

static volatile uint8_t s_rx_ring[UART_RX_RING_SIZE];
static volatile uint8_t s_rx_ring_head;
static volatile uint8_t s_rx_ring_tail;
static ugv_uart_parser_t s_parser;
static uint8_t s_telemetry_sequence;
static uint16_t s_control_rx_count;
static uint8_t s_last_control_flags;
static uint8_t s_last_enabled_mask;

static void set_all_targets_zero(void)
{
    for (motor_index_t motor = MOTOR_FRONT; motor < UGV_MOTOR_COUNT; ++motor) {
        (void)motor_control_set_target(motor, 0.0f);
    }
}

static bool targets_valid(const ugv_uart_control_t *command)
{
    const float limit = config_get()->max_target_rpm;
    return (float)command->front_rpm >= -limit &&
           (float)command->front_rpm <= limit &&
           (float)command->center_rpm >= -limit &&
           (float)command->center_rpm <= limit &&
           (float)command->rear_rpm >= -limit &&
           (float)command->rear_rpm <= limit;
}

static void handle_control(const ugv_uart_frame_t *frame)
{
    ugv_uart_control_t command;
    if (frame->type != UGV_UART_MSG_CONTROL ||
        !ugv_uart_decode_control(&command, frame->payload,
                                 frame->payload_size) ||
        command.target_role != UGV_NODE_ROLE_CODE ||
        !targets_valid(&command)) {
        return;
    }

    s_control_rx_count++;
    s_last_control_flags = command.flags;
    s_last_enabled_mask = command.enabled_mask;

    if ((command.flags & UGV_UART_CONTROL_FLAG_ESTOP) != 0u) {
        set_all_targets_zero();
        safety_set_motor_enable_mask(0u);
        safety_notify_command_received();
        safety_request_emergency_stop();
        return;
    }

    if ((command.flags & UGV_UART_CONTROL_FLAG_CLEAR_FAULT) != 0u) {
        set_all_targets_zero();
        safety_set_motor_enable_mask(0u);
        safety_notify_command_received();
        safety_clear_fault();
        return;
    }

    if ((command.flags & UGV_UART_CONTROL_FLAG_ARM) == 0u) {
        set_all_targets_zero();
        safety_set_motor_enable_mask(0u);
        safety_notify_command_received();
        safety_request_disarm();
        return;
    }

    if (motor_control_set_target(MOTOR_FRONT, (float)command.front_rpm) &&
        motor_control_set_target(MOTOR_CENTER, (float)command.center_rpm) &&
        motor_control_set_target(MOTOR_REAR, (float)command.rear_rpm)) {
        safety_set_motor_enable_mask(command.enabled_mask);
        safety_notify_command_received();
        safety_clear_emergency_stop();
        safety_request_arm();
    }
}

#ifdef UGV_OTA_APP
static void handle_firmware_command(const ugv_uart_frame_t *frame)
{
    ugv_fw_command_t command;
    const safety_state_t state = safety_get_state();
    const bool outputs_already_safe =
        state == SAFETY_STATE_DISABLED || state == SAFETY_STATE_FAULT ||
        state == SAFETY_STATE_EMERGENCY_STOP;
    if (frame->type != UGV_UART_MSG_FW_COMMAND ||
        !ugv_fw_decode_command(&command, frame->payload,
                               frame->payload_size) ||
        command.opcode != UGV_FW_COMMAND_ENTER ||
        command.target_node != (uint8_t)UGV_BOOT_NODE_ID ||
        command.value != UGV_FW_ENTER_MAGIC ||
        !outputs_already_safe) {
        return;
    }

    /* Never reset into the updater while an output can still be energized. */
    safety_set_motor_enable_mask(0u);
    motor_control_disable_all();
    ugv_boot_request_set_and_reset();
}
#endif

static int16_t rpm_as_i16(motor_index_t motor)
{
    const float rpm = motor_control_get_state(motor)->measured_rpm;
    if (rpm > (float)INT16_MAX) return INT16_MAX;
    if (rpm < (float)INT16_MIN) return INT16_MIN;
    return (int16_t)rpm;
}

static uint16_t current_as_ma(motor_index_t motor)
{
    const float current_a = motor_control_get_state(motor)->current_a;
    if (current_a <= 0.0f) return 0u;
    if (current_a >= 65.535f) return UINT16_MAX;
    return (uint16_t)(current_a * 1000.0f);
}

void uart_control_service_init(void)
{
    s_rx_ring_head = 0u;
    s_rx_ring_tail = 0u;
    s_telemetry_sequence = 0u;
    s_control_rx_count = 0u;
    s_last_control_flags = 0u;
    s_last_enabled_mask = 0u;
    ugv_uart_parser_init(&s_parser);

    /* CubeMX enables the USART2 IRQ in the NVIC, but HAL_UART_Init() does not
     * enable the peripheral's RX-not-empty interrupt until a HAL receive
     * operation is started. This service reads RDR directly in its ISR, so it
     * must explicitly enable RXNE after USART2 has been initialized. */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}

void uart_control_service_rx_isr(void)
{
    const uint32_t isr = huart2.Instance->ISR;
    if ((isr & UART_FLAG_RXNE) != 0u) {
        const uint8_t byte = (uint8_t)(huart2.Instance->RDR & 0xFFu);
        const uint8_t next_head =
            (uint8_t)((s_rx_ring_head + 1u) % UART_RX_RING_SIZE);
        if (next_head != s_rx_ring_tail) {
            s_rx_ring[s_rx_ring_head] = byte;
            s_rx_ring_head = next_head;
        }
    }
    if ((isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE)) != 0u) {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        __HAL_UART_CLEAR_NEFLAG(&huart2);
        __HAL_UART_CLEAR_FEFLAG(&huart2);
    }
}

void uart_control_service_poll(void)
{
    while (s_rx_ring_tail != s_rx_ring_head) {
        const uint8_t byte = s_rx_ring[s_rx_ring_tail];
        s_rx_ring_tail = (uint8_t)((s_rx_ring_tail + 1u) % UART_RX_RING_SIZE);
        ugv_uart_frame_t frame;
        if (ugv_uart_parser_push(&s_parser, byte, &frame)) {
            if (frame.type == UGV_UART_MSG_CONTROL) {
                handle_control(&frame);
#ifdef UGV_OTA_APP
            } else if (frame.type == UGV_UART_MSG_FW_COMMAND) {
                handle_firmware_command(&frame);
#endif
            }
        }
    }
}

void uart_control_service_publish_telemetry(void)
{
    uint8_t fault_mask = 0u;
    uint8_t valid_mask = 0u;
    for (motor_index_t motor = MOTOR_FRONT; motor < UGV_MOTOR_COUNT; ++motor) {
        const MotorState *state = motor_control_get_state(motor);
        const uint8_t bit = (uint8_t)(1u << (unsigned)motor);
        if (state->stalled || state->overcurrent || state->driver_fault ||
            !state->encoder_valid) {
            fault_mask |= bit;
        }
        if (state->encoder_valid) valid_mask |= bit;
        if (state->current_valid) valid_mask |= (uint8_t)(bit << 4u);
    }

    const ugv_uart_telemetry_t telemetry = {
        .node_role = UGV_NODE_ROLE_CODE,
        .safety_state = (uint8_t)safety_get_state(),
        .front_rpm = rpm_as_i16(MOTOR_FRONT),
        .center_rpm = rpm_as_i16(MOTOR_CENTER),
        .rear_rpm = rpm_as_i16(MOTOR_REAR),
        .front_current_ma = current_as_ma(MOTOR_FRONT),
        .center_current_ma = current_as_ma(MOTOR_CENTER),
        .rear_current_ma = current_as_ma(MOTOR_REAR),
        .fault_mask = fault_mask,
        .valid_mask = valid_mask,
        .control_rx_count = s_control_rx_count,
        .last_control_flags = s_last_control_flags,
        .last_enabled_mask = s_last_enabled_mask,
        .uptime_ms = HAL_GetTick(),
        .stack_free_bytes = stack_watermark_free_bytes(),
    };
    uint8_t payload[UGV_UART_TELEMETRY_PAYLOAD_SIZE];
    uint8_t frame[UGV_UART_MAX_FRAME_SIZE];
    if (!ugv_uart_encode_telemetry(payload, sizeof(payload), &telemetry)) {
        return;
    }
    const size_t frame_size = ugv_uart_encode_frame(
        frame, sizeof(frame), UGV_UART_MSG_TELEMETRY,
        s_telemetry_sequence++, payload, sizeof(payload));
    if (frame_size != 0u) {
        (void)HAL_UART_Transmit(&huart2, frame, (uint16_t)frame_size, 5u);
    }
}
