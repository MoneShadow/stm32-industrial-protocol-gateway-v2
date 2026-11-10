#include "stm32f1xx_hal.h"
#include "can.h"
#include "can_frame.h"
#include "can_port.h"

static void CAN_Port_Filter_Init(void) {
    CAN_FilterTypeDef f = {0};

    /* 32位MASK模式 因为只需要过滤1个ID 0x301 */
    f.FilterBank = 0;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh     = 0x301 << 5;
    f.FilterIdLow      = 0x0000;
    f.FilterMaskIdHigh = 0xFFE0;
    f.FilterMaskIdLow  = 0x0000;

    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation = CAN_FILTER_ENABLE;
    f.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) {
        Error_Handler();
    }
}

static can_port_result_t Can_Port_Start(void) {
    if (HAL_CAN_Start(&hcan) != HAL_OK) {  // 开启Can外设
        return CAN_PORT_ERROR;
    }
    if (HAL_CAN_ActivateNotification( &hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) { // 开中断
        return CAN_PORT_ERROR;
    }
    return CAN_PORT_OK;
}

can_frame_t frame = {0};
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef header = {0};
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, frame.data) != HAL_OK) {  // 取接收到的can帧
            break;
        }
        frame.id  = (header.IDE == CAN_ID_STD) ? header.StdId : header.ExtId;   // 根据ID类型填写ID
        frame.dlc = header.DLC; // 添加帧长度
        frame.flags = 0;        // 该网关目前只会接收到标准数据帧
        if (header.IDE == CAN_ID_EXT) {
            frame.flags |= CAN_FRAME_FLAG_IDE;
        }
        if (header.RTR == CAN_RTR_REMOTE) {
            frame.flags |= CAN_FRAME_FLAG_RTR;
        }
        HAL_GPIO_TogglePin(TEST_LED_GPIO_Port, TEST_LED_Pin);
    }
}

can_port_result_t Can_Port_Init(void) {
    CAN_Port_Filter_Init();
    if (Can_Port_Start() != CAN_PORT_OK) {
        return CAN_PORT_ERROR;
    }
    return CAN_PORT_OK;
}

can_port_result_t Can_Port_send(const can_frame_t *frame) {
    CAN_TxHeaderTypeDef header = {0};
    uint32_t used_mailbox;
    /* can帧数据包长度不大于8字节 */
    if (frame->dlc > 8U) {
        return CAN_PORT_INVALID_ARG;
    }
    /* 判断是标准帧还是扩展帧 */
    if (frame->flags & CAN_FRAME_FLAG_IDE) {
        /* 扩展帧的ID范围是 0x00000000 ~ 0x1FFFFFFF */
        if (frame->id > 0x1FFFFFFFU) {
            return CAN_PORT_INVALID_ARG;
        }
        header.ExtId = frame->id;
        header.IDE = CAN_ID_EXT;
    } else {
        /* 标准帧的ID范围是 0x000 ~ 0x7FF */
        if (frame->id > 0x7FFU) {
            return CAN_PORT_INVALID_ARG;
        }
        header.StdId = frame->id;
        header.IDE = CAN_ID_STD;
    }
    header.DLC = frame->dlc;
    header.RTR = frame->flags & 0x02 ? CAN_RTR_REMOTE : CAN_RTR_DATA;   // 判断是数据帧还是远程帧
    if (HAL_CAN_AddTxMessage(&hcan, &header, frame->data , &used_mailbox) != HAL_OK) {
        return CAN_PORT_BUSY;
    }
    return CAN_PORT_OK;
}