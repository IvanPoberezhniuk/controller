#include "can_control_service.h"

#include <stdint.h>

#include "board.h"
#include "configuration.h"
#include "fw_update_service.h"
#include "motor_control.h"
#include "safety.h"
#include "ugv_can_codec.h"
#include "ugv_can_protocol.h"
#include "ugv_fw_update_protocol.h"

extern FDCAN_HandleTypeDef hfdcan1;

static bool s_initialized;
static uint8_t s_telemetry_sequence;

static bool send_frame(uint16_t identifier, const uint8_t *payload,
                       uint32_t data_length)
{
    if (!s_initialized || payload == NULL ||
        HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0u) {
        return false;
    }

    const FDCAN_TxHeaderTypeDef header = {
        .Identifier = identifier,
        .IdType = FDCAN_STANDARD_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = data_length,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_OFF,
        .FDFormat = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0u,
    };
    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &header, payload) == HAL_OK;
}

static int16_t telemetry_rpm(motor_index_t motor)
{
    const float rpm = motor_control_get_state(motor)->measured_rpm;
    if (rpm > 32767.0f) return INT16_MAX;
    if (rpm < -32768.0f) return INT16_MIN;
    return (int16_t)rpm;
}

static void set_all_targets_zero(void)
{
    for (motor_index_t motor = MOTOR_FRONT; motor < UGV_MOTOR_COUNT; ++motor) {
        (void)motor_control_set_target(motor, 0.0f);
    }
}

static bool targets_valid(const ugv_can_wheel_targets_t *command)
{
    const float limit = config_get()->max_target_rpm;
    return (float)command->front_rpm >= -limit &&
           (float)command->front_rpm <= limit &&
           (float)command->center_rpm >= -limit &&
           (float)command->center_rpm <= limit &&
           (float)command->rear_rpm >= -limit &&
           (float)command->rear_rpm <= limit;
}

static void handle_wheel_targets(const uint8_t *payload)
{
    ugv_can_wheel_targets_t command;
    if (!ugv_can_decode_wheel_targets(&command, payload,
                                      UGV_CAN_WHEEL_TARGETS_LEFT_DLC) ||
        !targets_valid(&command)) {
        return;
    }

    if (motor_control_set_target(MOTOR_FRONT, (float)command.front_rpm) &&
        motor_control_set_target(MOTOR_CENTER, (float)command.center_rpm) &&
        motor_control_set_target(MOTOR_REAR, (float)command.rear_rpm)) {
        safety_set_motor_enable_mask(command.enabled_mask);
        safety_notify_command_received();
    }
}

static void handle_enable(const uint8_t *payload)
{
    ugv_can_system_enable_t command;
    if (!ugv_can_decode_system_enable(&command, payload,
                                      UGV_CAN_SYSTEM_ENABLE_DLC)) {
        return;
    }

    if (command.emergency_stop != 0u) {
        set_all_targets_zero();
        safety_set_motor_enable_mask(0u);
        safety_request_emergency_stop();
    } else if (command.enabled != 0u) {
        safety_clear_emergency_stop();
        safety_request_arm();
    } else {
        set_all_targets_zero();
        safety_set_motor_enable_mask(0u);
        safety_request_disarm();
    }
}

bool can_control_service_init(void)
{
#if defined(UGV_NODE_ROLE_LEFT)
    const uint16_t local_targets_id = UGV_CAN_MSG_WHEEL_TARGETS_LEFT;
#else
    const uint16_t local_targets_id = UGV_CAN_MSG_WHEEL_TARGETS_RIGHT;
#endif
    const FDCAN_FilterTypeDef control_filter = {
        .IdType = FDCAN_STANDARD_ID,
        .FilterIndex = 0u,
        .FilterType = FDCAN_FILTER_DUAL,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = local_targets_id,
        .FilterID2 = UGV_CAN_MSG_SYSTEM_ENABLE,
    };
    const FDCAN_FilterTypeDef update_filter = {
        .IdType = FDCAN_STANDARD_ID,
        .FilterIndex = 1u,
        .FilterType = FDCAN_FILTER_MASK,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = UGV_FW_CAN_ID_COMMAND,
        .FilterID2 = 0x7ffu,
    };

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &control_filter) != HAL_OK ||
        HAL_FDCAN_ConfigFilter(&hfdcan1, &update_filter) != HAL_OK ||
        HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK ||
        HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        return false;
    }

    s_initialized = true;
    return true;
}

void can_control_service_publish_telemetry(void)
{
    uint8_t payload[UGV_CAN_TELEMETRY_LEFT_DLC] = {0};
    const ugv_can_motor_telemetry_t telemetry = {
        .sequence = s_telemetry_sequence++,
        .safety_state = (uint8_t)safety_get_state(),
        .front_rpm = telemetry_rpm(MOTOR_FRONT),
        .center_rpm = telemetry_rpm(MOTOR_CENTER),
        .rear_rpm = telemetry_rpm(MOTOR_REAR),
    };
    if (ugv_can_encode_motor_telemetry(payload, sizeof(payload), &telemetry)) {
        (void)send_frame(UGV_NODE_TELEMETRY_ID, payload, FDCAN_DLC_BYTES_8);
    }
}

void can_control_service_poll(void)
{
    if (!s_initialized) {
        return;
    }

    while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0u) {
        FDCAN_RxHeaderTypeDef header;
        uint8_t payload[8] = {0};
        if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0,
                                   &header, payload) != HAL_OK ||
            header.IdType != FDCAN_STANDARD_ID ||
            header.RxFrameType != FDCAN_DATA_FRAME) {
            continue;
        }

#if defined(UGV_NODE_ROLE_LEFT)
        const uint32_t local_targets_id = UGV_CAN_MSG_WHEEL_TARGETS_LEFT;
#else
        const uint32_t local_targets_id = UGV_CAN_MSG_WHEEL_TARGETS_RIGHT;
#endif
        if (header.Identifier == local_targets_id &&
            header.DataLength == FDCAN_DLC_BYTES_8) {
            handle_wheel_targets(payload);
        } else if (header.Identifier == UGV_CAN_MSG_SYSTEM_ENABLE &&
                   header.DataLength == FDCAN_DLC_BYTES_2) {
            handle_enable(payload);
        } else if (header.Identifier == UGV_FW_CAN_ID_COMMAND &&
                   header.DataLength == FDCAN_DLC_BYTES_8) {
            fw_update_service_handle_frame((uint16_t)header.Identifier,
                                           payload, sizeof(payload));
        }
    }
}
