/* Modbus 接收任务：TCP 事件 → MBAP/PDU → 语义请求 → GatewayTask 队列。 */
#include "FreeRTOS.h"
#include "task.h"

#include "gateway.h"
#include "gateway_internal.h"
#include "net_port.h"
#include "modbus_tcp.h"
#include "modbus_pdu.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool used;
    int fd;
    uint32_t conn_id;
    modbus_tcp_conn_t modbus;
} gateway_session_t;

static gateway_session_t s_sessions[NET_PORT_MAX_CLIENTS];

/* 根据fd找session会话 */
static gateway_session_t *Gateway_SessionFind(int fd, uint32_t conn_id) {
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        if (s_sessions[i].used && s_sessions[i].fd == fd && s_sessions[i].conn_id == conn_id) {
            return &s_sessions[i];
        }
    }
    return NULL;
}

/* 删除客户端session */
static void Gateway_SessionFree(gateway_session_t *session) {
    if (session == NULL) {
        return;
    }

    Modbus_Tcp_Init(&session->modbus);
    session->fd = -1;
    session->used = false;
}

static gateway_session_t *Gateway_SessionAlloc(int fd, uint32_t conn_id) {
    /* 第一遍：清掉所有同 fd 的旧 session */
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        if (s_sessions[i].used && fd == s_sessions[i].fd && s_sessions[i].conn_id != conn_id) {
            Gateway_SessionFree(&s_sessions[i]);    // 清除旧连接的残留 session
        }
    }
    /* 第二遍：找空槽分配 */
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        if (!s_sessions[i].used) {
            s_sessions[i].used = true;
            s_sessions[i].fd = fd;
            s_sessions[i].conn_id = conn_id;
            Modbus_Tcp_Init(&s_sessions[i].modbus);
            return &s_sessions[i];
        }
    }
    return NULL;
}

static void Gateway_HandleModbusData(int fd, uint32_t conn_id, const uint8_t *data, uint16_t len) {
    gateway_session_t *session = Gateway_SessionFind(fd, conn_id);
    modbus_tcp_message_t request;

    if (session == NULL || data == NULL || len == 0U) {
        return;
    }

    if (Modbus_Tcp_Feed(&session->modbus, data, len) != MODBUS_TCP_OK) {
        Modbus_Tcp_Init(&session->modbus);
        return;
    }

    while (Modbus_Tcp_Poll(&session->modbus, &request) == MODBUS_TCP_OK) {
        gateway_modbus_msg_t message;

        message.fd = fd;
        message.conn_id = conn_id;
        message.transaction_id = request.transaction_id;
        message.unit_id = request.unit_id;
        message.parse_result = Modbus_Pdu_Parse(request.pdu, request.pdu_len, &message.request);

        if (Gateway_PostModbus(&message) != pdPASS) {
            /* 队列满：暂时丢弃该请求，避免 Modbus task 阻塞 */
        }
    }
}

void Gateway_ModbusRxTask(void *arg) {
    (void)arg;

    for (;;) {
        net_port_event_t event;

        if (Net_Port_GetEvent(&event, UINT32_MAX) != NET_PORT_OK) {
            continue;
        }

        switch (event.type) {
        case NET_PORT_EVENT_CONNECTED:
            (void)Gateway_SessionAlloc(event.fd, event.conn_id);
            break;

        case NET_PORT_EVENT_DATA:
            Gateway_HandleModbusData(event.fd, event.conn_id, event.data, event.len);
            break;

        case NET_PORT_EVENT_DISCONNECTED:
            Gateway_SessionFree(Gateway_SessionFind(event.fd, event.conn_id));
            break;

        default:
            break;
        }
    }
}
