#include "fw_update_service.h"

#include <stdint.h>

#include "app_main.h"
#include "configuration.h"
#include "ugv_fw_update_protocol.h"

#if defined(HAL_FDCAN_MODULE_ENABLED)

#include "board.h"

extern FDCAN_HandleTypeDef hfdcan1;

static bool s_initialized;

static bool send_error(uint8_t session, uint8_t detail)
{
    uint16_t identifier = 0u;
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    const ugv_fw_status_t status = {
        .code = UGV_FW_STATUS_ERROR,
        .session = session,
        .detail = detail,
        .protocol_version = UGV_FW_PROTOCOL_VERSION,
        .value = 0u,
    };
    if (!ugv_fw_status_id_for_node(UGV_NODE_CAN_ID, &identifier) ||
        !ugv_fw_encode_status(payload, sizeof(payload), &status)) {
        return false;
    }

    FDCAN_TxHeaderTypeDef header = {
        .Identifier = identifier,
        .IdType = FDCAN_STANDARD_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = FDCAN_DLC_BYTES_8,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_OFF,
        .FDFormat = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0u,
    };
    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &header, payload) == HAL_OK;
}

bool fw_update_service_init(void)
{
    FDCAN_FilterTypeDef filter = {
        .IdType = FDCAN_STANDARD_ID,
        .FilterIndex = 0u,
        .FilterType = FDCAN_FILTER_MASK,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = UGV_FW_CAN_ID_COMMAND,
        .FilterID2 = 0x7ffu,
    };

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK ||
        HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK ||
        HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        return false;
    }
    s_initialized = true;
    return true;
}

void fw_update_service_poll(void)
{
    if (!s_initialized ||
        HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) == 0u) {
        return;
    }

    FDCAN_RxHeaderTypeDef header;
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &header, payload) !=
            HAL_OK ||
        header.Identifier != UGV_FW_CAN_ID_COMMAND ||
        header.IdType != FDCAN_STANDARD_ID ||
        header.RxFrameType != FDCAN_DATA_FRAME ||
        header.DataLength != FDCAN_DLC_BYTES_8) {
        return;
    }

    ugv_fw_command_t command;
    if (!ugv_fw_decode_command(&command, payload, sizeof(payload)) ||
        command.target_node != UGV_NODE_CAN_ID) {
        return;
    }

    if (command.opcode == UGV_FW_COMMAND_ENTER) {
        if (!app_main_request_bootloader()) {
            (void)send_error(command.session, UGV_FW_ERROR_BAD_STATE);
        }
    }
}

#else

bool fw_update_service_init(void)
{
    return false;
}

void fw_update_service_poll(void)
{
}

#endif /* HAL_FDCAN_MODULE_ENABLED */
