#include "ugv_boot_can.h"

#include <string.h>

#include "stm32g4xx_hal.h"

#define BOOT_CAN_TX_TIMEOUT_MS 100u

static FDCAN_HandleTypeDef s_fdcan;

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *handle)
{
    if (handle == NULL || handle->Instance != FDCAN1) {
        return;
    }

    __HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PCLK1);
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef pins = {
        .Pin = GPIO_PIN_11 | GPIO_PIN_12,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF9_FDCAN1,
    };
    HAL_GPIO_Init(GPIOA, &pins);
}

void HAL_FDCAN_MspDeInit(FDCAN_HandleTypeDef *handle)
{
    if (handle == NULL || handle->Instance != FDCAN1) {
        return;
    }
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
    __HAL_RCC_FDCAN_CLK_DISABLE();
}

static bool configure_exact_filter(uint32_t index, uint16_t identifier)
{
    FDCAN_FilterTypeDef filter = {
        .IdType = FDCAN_STANDARD_ID,
        .FilterIndex = index,
        .FilterType = FDCAN_FILTER_MASK,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = identifier,
        .FilterID2 = 0x7ffu,
    };
    return HAL_FDCAN_ConfigFilter(&s_fdcan, &filter) == HAL_OK;
}

bool ugv_boot_can_init(uint8_t node_id)
{
    uint16_t data_identifier = 0u;
    if (!ugv_fw_data_id_for_node(node_id, &data_identifier)) {
        return false;
    }

    memset(&s_fdcan, 0, sizeof(s_fdcan));
    s_fdcan.Instance = FDCAN1;
    s_fdcan.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    s_fdcan.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    s_fdcan.Init.Mode = FDCAN_MODE_NORMAL;
    s_fdcan.Init.AutoRetransmission = ENABLE;
    s_fdcan.Init.TransmitPause = ENABLE;
    s_fdcan.Init.ProtocolException = DISABLE;

    /* PCLK1 is the reset-default HSI16 (16 MHz):
     * 16 MHz / prescaler 2 / (1 sync + 13 seg1 + 2 seg2) = 500 kbit/s,
     * sample point = 87.5%. */
    s_fdcan.Init.NominalPrescaler = 2u;
    s_fdcan.Init.NominalSyncJumpWidth = 2u;
    s_fdcan.Init.NominalTimeSeg1 = 13u;
    s_fdcan.Init.NominalTimeSeg2 = 2u;
    s_fdcan.Init.DataPrescaler = 2u;
    s_fdcan.Init.DataSyncJumpWidth = 2u;
    s_fdcan.Init.DataTimeSeg1 = 13u;
    s_fdcan.Init.DataTimeSeg2 = 2u;
    s_fdcan.Init.StdFiltersNbr = 2u;
    s_fdcan.Init.ExtFiltersNbr = 0u;
    s_fdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&s_fdcan) != HAL_OK ||
        !configure_exact_filter(0u, UGV_FW_CAN_ID_COMMAND) ||
        !configure_exact_filter(1u, data_identifier) ||
        HAL_FDCAN_ConfigGlobalFilter(&s_fdcan, FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK ||
        HAL_FDCAN_Start(&s_fdcan) != HAL_OK) {
        return false;
    }
    return true;
}

bool ugv_boot_can_receive(ugv_boot_can_frame_t *frame)
{
    if (frame == NULL ||
        HAL_FDCAN_GetRxFifoFillLevel(&s_fdcan, FDCAN_RX_FIFO0) == 0u) {
        return false;
    }

    FDCAN_RxHeaderTypeDef header;
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    if (HAL_FDCAN_GetRxMessage(&s_fdcan, FDCAN_RX_FIFO0, &header, payload) !=
            HAL_OK ||
        header.IdType != FDCAN_STANDARD_ID ||
        header.RxFrameType != FDCAN_DATA_FRAME ||
        header.DataLength != FDCAN_DLC_BYTES_8 ||
        header.Identifier > 0x7ffu) {
        return false;
    }
    frame->identifier = (uint16_t)header.Identifier;
    memcpy(frame->payload, payload, sizeof(frame->payload));
    return true;
}

bool ugv_boot_can_send_status(uint8_t node_id,
                              const ugv_fw_status_t *status)
{
    uint16_t identifier = 0u;
    uint8_t payload[UGV_FW_FRAME_DLC] = {0};
    if (!ugv_fw_status_id_for_node(node_id, &identifier) ||
        !ugv_fw_encode_status(payload, sizeof(payload), status)) {
        return false;
    }

    const uint32_t start = HAL_GetTick();
    while (HAL_FDCAN_GetTxFifoFreeLevel(&s_fdcan) == 0u) {
        IWDG->KR = 0xaaaau;
        if ((HAL_GetTick() - start) >= BOOT_CAN_TX_TIMEOUT_MS) {
            return false;
        }
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
    return HAL_FDCAN_AddMessageToTxFifoQ(&s_fdcan, &header, payload) == HAL_OK;
}
