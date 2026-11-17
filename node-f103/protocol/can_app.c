#include "can_app.h"

#include "device.h"

#include <string.h>

/* 解码FAN LED 写命令函数 */
int8_t Can_App_Deco(can_frame_t* frame, can_protocol_msg_t *deco_msg) {
    if (frame == NULL || deco_msg == NULL || frame->id != 0x301 || frame->flags) {
        return -1;
    }
    if (frame->data[0] == 0x01) {   // 写风扇命令
        deco_msg->commandcode = frame->data[0];
        deco_msg->fan_rpm = (uint16_t)((uint16_t)frame->data[1] |
                                       ((uint16_t)frame->data[2] << 8));
        deco_msg->seq = frame->data[3];
    }
    else if (frame->data[0] == 0x02) {  // 写LED命令
        deco_msg->commandcode = frame->data[0];
        deco_msg->led_state = frame->data[1];
        deco_msg->seq = frame->data[3];
    }
    else {
        return -1;
    }
    return 0;
}

/* 编码ACK结构体 0成功 1失败 2非法 */
int8_t Can_App_Ack_Code(const can_protocol_msg_t *deco_msg, can_protocol_ack_t *ack, uint8_t result) {
    if (deco_msg == NULL || ack == NULL || result > 2U) {
        return -1;
    }
    ack->commandcode = deco_msg->commandcode;
    ack->seq = deco_msg->seq;
    ack->cmd_result = result;
    return 0;
}

/* 编码0x401 ACK帧 */
int8_t Can_App_Ack_Frame_Code(const can_protocol_msg_t *deco_msg, uint8_t result, can_frame_t *frame) {
    can_protocol_ack_t ack;

    if (frame == NULL || Can_App_Ack_Code(deco_msg, &ack, result) != 0) {
        return -1;
    }

    *frame = (can_frame_t){0};
    frame->id = 0x401;
    frame->dlc = 3;
    frame->flags = 0;
    frame->data[0] = ack.commandcode;
    frame->data[1] = ack.cmd_result;
    frame->data[2] = ack.seq;
    return 0;
}

/* 编码0x201 心跳帧 */
int8_t Can_App_Heart_Code(uint8_t node_id, uint8_t seq, can_frame_t *frame) {
    if (frame == NULL) {
        return -1;
    }

    *frame = (can_frame_t){0};
    frame->id = 0x201;
    frame->dlc = 2;
    frame->flags = 0;
    frame->data[0] = node_id;
    frame->data[1] = seq;
    return 0;
}

/* 0x101 状态编码函数：温度 ×10 / 光照 / 风扇转速 / LED，多字节小端 */
int8_t Can_App_State_Code(const device_model_t *device_model, can_frame_t *frame) {
    if (device_model == NULL || frame == NULL) {
        return -1;
    }
    frame->id = 0x101;
    frame->dlc = 8;
    frame->flags = 0;
    frame->data[0] = (uint8_t)(device_model->tempture & 0xFFU);
    frame->data[1] = (uint8_t)(device_model->tempture >> 8);
    frame->data[2] = (uint8_t)(device_model->light & 0xFFU);
    frame->data[3] = (uint8_t)(device_model->light >> 8);
    frame->data[4] = (uint8_t)(device_model->fan_rpm & 0xFFU);
    frame->data[5] = (uint8_t)(device_model->fan_rpm >> 8);
    frame->data[6] = (uint8_t)(device_model->led_state & 0xFFU);
    frame->data[7] = 0;
    return 0;
}