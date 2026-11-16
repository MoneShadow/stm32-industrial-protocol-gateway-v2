/* 原始 TCP 收发服务：socket / select / 事件队列 / TX 队列 */
#ifndef NET_PORT_H
#define NET_PORT_H

#include <stdint.h>

#define NET_PORT_MAX_CLIENTS      4      /* 最大客户端数量 */
#define NET_PORT_RX_CHUNK         256    /* 一次 DATA 事件的最大字节数 */
#define NET_PORT_EVENT_QUEUE_LEN  32     /* 连接/数据/断开事件队列深度 */
#define NET_PORT_TX_CHUNK         260    /* 单条发送数据的最大字节数 */
#define NET_PORT_TX_QUEUE_LEN     4      /* 每个客户端的发送队列深度 */

typedef enum {
    NET_PORT_OK = 0,
    NET_PORT_ERROR,
    NET_PORT_TIMEOUT,
    NET_PORT_BUSY,
    NET_PORT_INVALID_ARG
} net_port_result_t;

typedef enum {
    NET_PORT_EVENT_CONNECTED = 0,   /* 新 TCP 客户端 */
    NET_PORT_EVENT_DATA,            /* 收到原始字节 */
    NET_PORT_EVENT_DISCONNECTED     /* 客户端已关闭 */
} net_port_event_type_t;

typedef struct {
    net_port_event_type_t type;
    int fd;
    uint32_t conn_id;
    uint16_t len;
    uint8_t data[NET_PORT_RX_CHUNK]; /* CONNECTED/DISCONNECTED 时 len=0 */
} net_port_event_t;

/* 在 lwIP 初始化完成后调用；只创建事件队列，不创建任务。 */
net_port_result_t Net_Port_Init(uint16_t port);

/*
 * 取一个连接生命周期/数据事件。
 * 事件按 CONNECTED -> DATA... -> DISCONNECTED 的顺序入队。
 * timeout_ms = UINT32_MAX 表示永久等待。
 */
net_port_result_t Net_Port_GetEvent(net_port_event_t *event, uint32_t timeout_ms);

/* 向指定客户端排队发送原始字节；返回 BUSY 表示该客户端发送队列已满。 */
net_port_result_t Net_Port_Send(int fd, uint32_t conn_id, const uint8_t *data, uint16_t len);

/* 由 app 创建任务；内部负责 listen/accept/select/recv/send。 */
void NetServerTask(void *arg);

#endif
