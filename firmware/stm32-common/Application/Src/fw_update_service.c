#include "fw_update_service.h"

#include "app_main.h"
#include "configuration.h"
#include "ugv_fw_update_protocol.h"

#if defined(HAL_FDCAN_MODULE_ENABLED)

#include "board.h"

extern FDCAN_HandleTypeDef hfdcan1;

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

void fw_update_service_handle_frame(uint16_t identifier,
                                    const uint8_t *payload,
                                    size_t size)
{
    if (identifier != UGV_FW_CAN_ID_COMMAND || payload == NULL ||
        size != UGV_FW_FRAME_DLC) {
        return;
    }

    ugv_fw_command_t command;
    if (!ugv_fw_decode_command(&command, payload, size) ||
        command.target_node != UGV_NODE_CAN_ID) {
        return;
    }

    if (command.opcode == UGV_FW_COMMAND_ENTER &&
        !app_main_request_bootloader()) {
        (void)send_error(command.session, UGV_FW_ERROR_BAD_STATE);
    }
}

#else

void fw_update_service_handle_frame(uint16_t identifier,
                                    const uint8_t *payload,
                                    size_t size)
{
    (void)identifier;
    (void)payload;
    (void)size;
}

#endif /* HAL_FDCAN_MODULE_ENABLED */
