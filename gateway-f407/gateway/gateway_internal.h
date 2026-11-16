#ifndef GATEWAY_INTERNAL_H
#define GATEWAY_INTERNAL_H

#include "FreeRTOS.h"
#include "queue.h"

#include "can_protocol.h"
#include "modbus_pdu.h"

/* Modbus 接收任务交给 GatewayTask 的语义请求 data -> 字段 */
typedef struct {
    int fd;
    uint32_t conn_id;
    uint16_t transaction_id;
    uint8_t unit_id;
    modbus_pdu_result_t parse_result;
    modbus_pdu_request_t request;
} gateway_modbus_msg_t;

/* CAN 接收任务解码后交给 GatewayTask 的事件 data -> 字段  */
typedef enum {
    GATEWAY_CAN_STATUS = 0,
    GATEWAY_CAN_HEARTBEAT,
    GATEWAY_CAN_ACK
} gateway_can_msg_type_t;

typedef struct {
    gateway_can_msg_type_t type;
    union {
        device_state_t status;
        device_heart_t heart;
        device_ack_t ack;
    } data;
} gateway_can_msg_t;

/* 生产者 API；Queue 满时返回 pdFALSE。 */
BaseType_t Gateway_PostModbus(const gateway_modbus_msg_t *msg);
BaseType_t Gateway_PostCan(const gateway_can_msg_t *msg);

#endif
