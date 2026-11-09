/* 过滤器、启动、发送、接收、ISR、队列 */
/* 该文件向外保留接收到的原数据 不对数据进行任何二次加工 */
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "can.h"
#include "can_port.h"

/* 想要接收can帧，要先配置过滤器 本网关主要接收 下位机状态0x101 心跳0x201 命令ACK0x401 */
static uint32_t can_std_id_to_filter(uint32_t id) {
    return (id << 5) & 0xFFFFU;
}
static void CAN_Port_Filter_Init(void) {
    CAN_FilterTypeDef f = {0};

    /* 16位清单模式 因为只需要过滤3个ID */
    f.FilterBank = 0;
    f.FilterMode = CAN_FILTERMODE_IDLIST;
    f.FilterScale = CAN_FILTERSCALE_16BIT;

    /*
     * 16-bit list mode 下，这四个字段都是“ID”，不是 mask。
     * 每个字段占一个 16 位槽位。
     */
    f.FilterIdLow      = can_std_id_to_filter(0x101);
    f.FilterMaskIdLow  = can_std_id_to_filter(0x201);
    f.FilterIdHigh     = can_std_id_to_filter(0x401);

    /*
     * 第四个槽位没有新 ID，填一个重复 ID 占位。
     * 重复 ID 不会引入新的可接收 ID。
     */
    f.FilterMaskIdHigh = can_std_id_to_filter(0x401);

    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation = CAN_FILTER_ENABLE;
    f.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &f) != HAL_OK) {
        Error_Handler();
    }
}

static QueueHandle_t s_can_rx_queue;
static can_port_result_t Can_Port_RxQueue_Init(void) {
    s_can_rx_queue = xQueueCreate(16, sizeof(can_frame_t)); // 申请收帧队列
    if (s_can_rx_queue == NULL) {
        return CAN_PORT_ERROR;
    }
    CAN_Port_Filter_Init(); // 初始化过滤器
    return CAN_PORT_OK;
}

static can_port_result_t Can_Port_Start(void) {
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {  // 开启Can外设
        return CAN_PORT_ERROR;
    }
    if (HAL_CAN_ActivateNotification( &hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) { // 开中断
        return CAN_PORT_ERROR;
    }
    return CAN_PORT_OK;
}

/* 重申，该文件只原样接收并返回can帧，不做二次处理 */
static uint32_t RxFIFOFULLED;
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef header = {0};
    can_frame_t frame = {0};
    BaseType_t woken = pdFALSE;
    if (hcan != &hcan1) {
        return;
    }
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
        /* 消息入队 */
        if (xQueueSendFromISR(s_can_rx_queue, &frame, &woken) != pdPASS) {
            /* 队列满，用一个静态变量进行丢帧统计，仅供调试使用，如果需要对外调用，请使用对外提供的查询函数 */
            RxFIFOFULLED++;
        }
    }
    portYIELD_FROM_ISR(woken);
}

/* 对外暴露的初始化API */
can_port_result_t Can_Port_Init(void) {
    if (Can_Port_RxQueue_Init() != 0) {
        return CAN_PORT_ERROR;
    }
    if (Can_Port_Start() != 0) {
        return CAN_PORT_ERROR;
    }
    return CAN_PORT_OK;
}

/* 对外暴露的接收API */
can_port_result_t Can_Port_recv(can_frame_t *frame, uint32_t timeout_ms) {
    TickType_t ticks;
    if (timeout_ms == UINT32_MAX) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout_ms);
    }
    if (xQueueReceive(s_can_rx_queue, frame, ticks) != pdPASS) {
        return CAN_PORT_TIMEOUT;
    }
    return CAN_PORT_OK;
}

/* 对外暴露的发送API */
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
    if (HAL_CAN_AddTxMessage(&hcan1, &header, frame->data , &used_mailbox) != HAL_OK) {
        return CAN_PORT_BUSY;
    }
    return CAN_PORT_OK;
}

/* 对外暴露的查询丢帧次数API */
uint32_t Can_Port_GetDropCount(void) {
    return RxFIFOFULLED;
}
