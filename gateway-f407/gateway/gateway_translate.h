#ifndef GATEWAY_TRANSLATE_H
#define GATEWAY_TRANSLATE_H

#include "FreeRTOS.h"
#include "queue.h"
#include "can_frame.h"
#include "gateway_internal.h"

#include <stdint.h>

/* modbus <-> can 需要保留两份 同意不同译的信息 */
typedef struct {
    gateway_modbus_msg_t modbus_msg;
    union {
        can_frame_t can_frame;
        /* 保留 */
    };
} gateway_translate_t;

/* 由 Gateway_Init 创建；Gateway 和 command_manager 直接访问。 */
extern QueueHandle_t g_translate_queue;

#endif
