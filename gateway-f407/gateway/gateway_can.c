/* CAN 接收任务：Can_Port 原始帧 → can_app 解码 → GatewayTask 队列。 */
#include "FreeRTOS.h"
#include "task.h"

#include "gateway.h"
#include "gateway_internal.h"
#include "can_port.h"
#include "can_app.h"

#include <stdint.h>

void Gateway_CanRxTask(void *arg) {
    (void)arg;

    for (;;) {
        can_frame_t frame;
        gateway_can_msg_t message;

        if (Can_Port_recv(&frame, UINT32_MAX) != CAN_PORT_OK) {
            continue;
        }

        switch (frame.id) {
        case 0x101:
            if (Can_App_States_Deco(&frame, &message.data.status) != CAN_APP_OK) {
                continue;
            }
            message.type = GATEWAY_CAN_STATUS;
            break;

        case 0x201:
            if (Can_App_Heart_Deco(&frame, &message.data.heart) != CAN_APP_OK) {
                continue;
            }
            message.type = GATEWAY_CAN_HEARTBEAT;
            break;

        case 0x401:
            if (Can_App_ACK_Deco(&frame, &message.data.ack) != CAN_APP_OK) {
                continue;
            }
            message.type = GATEWAY_CAN_ACK;
            break;

        default:
            continue;
        }

        if (Gateway_PostCan(&message) != pdPASS) {
            /* 队列满：暂时丢弃该事件，避免 CAN task 阻塞。 */
        }
    }
}
