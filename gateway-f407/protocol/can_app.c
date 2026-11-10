/* 0x101/0x201/0x301/0x401 编解码 */
#include "can_app.h"
#include <stddef.h>

/* 搞清楚边界，本文件只负责编解码，不负责命令的最终发送 */

/* 
    先编码0x301命令 基本格式为：命令码 状态 命令序号
    1表示风扇命令 用2个字节表达转速(小端序) 一个字节装载序号 比如 1 3200 1 00 表示操作风扇转速为3200转命令序号1
    2表示led命令  用一个字节表示开关状态0/1 关/开 三个字节装载序号 其余位保留 比如 2 1 1 00000 表示打开led命令序号为1 
*/
can_app_result_t Can_App_Fan_Code(uint16_t rpm, uint8_t seq, can_frame_t *frame) {
    if (frame == NULL || rpm > 3000U) {
        return CAN_APP_INVALID_ARG;
    }
    frame->id = 0x301;
    frame->dlc = 8;
    frame->flags = 0;
    frame->data[0] = 1;
    frame->data[1] = (uint8_t)(rpm & 0xFF);             // 转速低位
    frame->data[2] = (uint8_t)((rpm >> 8) & 0xFF);      // 转速高位
    frame->data[3] = seq;                               // 命令序号
    frame->data[4] = 0;
    frame->data[5] = 0;
    frame->data[6] = 0;
    frame->data[7] = 0;
    return CAN_APP_OK;
}

can_app_result_t Can_App_LED_Code(uint8_t state, uint8_t seq, can_frame_t *frame) {
    if (frame == NULL || state > 1U) {
        return CAN_APP_INVALID_ARG;
    }
    frame->id = 0x301;
    frame->dlc = 8;
    frame->flags = 0;
    frame->data[0] = 2;
    frame->data[1] = state;  // LED状态
    frame->data[2] = 0;
    frame->data[3] = seq;    // 命令序号
    frame->data[4] = 0;
    frame->data[5] = 0;
    frame->data[6] = 0;
    frame->data[7] = 0;
    return CAN_APP_OK;
}

/* 先搞清楚0x101状态解码函数的边界 依旧是只负责解码 不负责更新模型 */
/* 解码命令 0x101 状态命令 */
can_app_result_t Can_App_States_Deco(const can_frame_t *frame, device_state_t *status) {
    /*
        Byte 0-1  温度 ×10，小端
        Byte 2-3  光照，小端
        Byte 4-5  风扇转速，小端
        Byte 6    LED状态
        Byte 7    保留
    */
    if (frame == NULL || status == NULL) {
        return CAN_APP_INVALID_ARG;
    }
    if (frame->id != 0x101) {
        return CAN_APP_BAD_ID;
    }
    if (frame->dlc != 8) {
        return CAN_APP_BAD_DLC;
    }
    if (frame->flags & (CAN_FRAME_FLAG_IDE | CAN_FRAME_FLAG_RTR)) {
        return CAN_APP_BAD_FRAME;
    }
    uint16_t temperature, light, fan_rpm, led_state;
    temperature = (uint16_t)frame->data[1] << 8 | (uint16_t)frame->data[0];
    light = (uint16_t)frame->data[3] << 8 | (uint16_t)frame->data[2];
    fan_rpm = (uint16_t)frame->data[5] << 8 | (uint16_t)frame->data[4];
    led_state = (uint16_t)frame->data[6];
    status->temperature = temperature;
    status->light = light;
    status->fan_rpm = fan_rpm;
    status->led_state = led_state;
    return CAN_APP_OK;
}

/* 0x201 心跳命令 */
can_app_result_t Can_App_Heart_Deco(const can_frame_t *frame, device_heart_t *Heart) {
    /*
        Byte 0  Node ID = 1
        Byte 1  心跳序号
    */
    if (frame == NULL || Heart == NULL) {
        return CAN_APP_INVALID_ARG;
    }
    if (frame->id != 0x201) {
        return CAN_APP_BAD_ID;
    }
    if (frame->dlc != 2) {
        return CAN_APP_BAD_DLC;
    }
    if (frame->flags & (CAN_FRAME_FLAG_IDE | CAN_FRAME_FLAG_RTR)) {
        return CAN_APP_BAD_FRAME;
    }
    Heart->node_id = frame->data[0];
    Heart->heart_seq = frame->data[1];
    return CAN_APP_OK;
}

/* 0x401 命令ACK */
can_app_result_t Can_App_ACK_Deco(const can_frame_t *frame, device_ack_t *Ack) {
    /*
        Byte 0  原命令码
        Byte 1  结果：0 成功，1 失败，2 非法
        Byte 2  原命令序号
    */
    if (frame == NULL || Ack == NULL) {
        return CAN_APP_INVALID_ARG;
    }
    if (frame->id != 0x401) {
        return CAN_APP_BAD_ID;
    }
    if (frame->dlc != 3) {
        return CAN_APP_BAD_DLC;
    }
    if (frame->flags & (CAN_FRAME_FLAG_IDE | CAN_FRAME_FLAG_RTR)) {
        return CAN_APP_BAD_FRAME;
    }
    Ack->commandcode = frame->data[0];
    Ack->result = frame->data[1];
    Ack->seq = frame->data[2];
    return CAN_APP_OK;
}