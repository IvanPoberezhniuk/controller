#include "ugv_boot_uart.h"

#include <string.h>

#include "stm32g4xx_hal.h"

#define BOOT_UART_TX_TIMEOUT_MS 100u

static UART_HandleTypeDef s_uart;
static ugv_uart_parser_t s_parser;
static uint8_t s_tx_sequence;

void HAL_UART_MspInit(UART_HandleTypeDef *handle)
{
    if (handle == NULL || handle->Instance != USART2) {
        return;
    }

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef pins = {
        .Pin = GPIO_PIN_2 | GPIO_PIN_3,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF7_USART2,
    };
    HAL_GPIO_Init(GPIOA, &pins);
}

bool ugv_boot_uart_init(void)
{
    memset(&s_uart, 0, sizeof(s_uart));
    s_uart.Instance = USART2;
    s_uart.Init.BaudRate = UGV_UART_BAUD_RATE;
    s_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_uart.Init.StopBits = UART_STOPBITS_1;
    s_uart.Init.Parity = UART_PARITY_NONE;
    s_uart.Init.Mode = UART_MODE_TX_RX;
    s_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    s_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    ugv_uart_parser_init(&s_parser);
    s_tx_sequence = 0u;
    return HAL_UART_Init(&s_uart) == HAL_OK;
}

bool ugv_boot_uart_receive(ugv_uart_frame_t *frame)
{
    if (frame == NULL) {
        return false;
    }

    const uint32_t isr = USART2->ISR;
    if ((isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE)) != 0u) {
        __HAL_UART_CLEAR_OREFLAG(&s_uart);
        __HAL_UART_CLEAR_NEFLAG(&s_uart);
        __HAL_UART_CLEAR_FEFLAG(&s_uart);
    }
    if ((USART2->ISR & USART_ISR_RXNE_RXFNE) == 0u) {
        return false;
    }
    const uint8_t byte = (uint8_t)(USART2->RDR & 0xffu);
    return ugv_uart_parser_push(&s_parser, byte, frame);
}

bool ugv_boot_uart_send_status(const ugv_fw_status_t *status)
{
    uint8_t payload[UGV_FW_FRAME_DLC];
    uint8_t frame[UGV_UART_MAX_FRAME_SIZE];
    if (status == NULL ||
        !ugv_fw_encode_status(payload, sizeof(payload), status)) {
        return false;
    }
    const size_t frame_size = ugv_uart_encode_frame(
        frame, sizeof(frame), UGV_UART_MSG_FW_STATUS, s_tx_sequence++,
        payload, sizeof(payload));
    return frame_size != 0u &&
           HAL_UART_Transmit(&s_uart, frame, (uint16_t)frame_size,
                             BOOT_UART_TX_TIMEOUT_MS) == HAL_OK;
}
