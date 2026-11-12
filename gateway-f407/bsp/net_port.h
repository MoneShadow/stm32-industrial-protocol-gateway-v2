/* 原始 TCP 收发服务：socket / select / RX 队列 / TX 队列 */
#ifndef NET_PORT_H
#define NET_PORT_H

#include <stdint.h>

#define NET_PORT_MAX_CLIENTS     4      // 最大建立连接的客户端数量
#define NET_PORT_RX_CHUNK        256    // 一次可接收的最大数据量
#define NET_PORT_RX_QUEUE_LEN    8      // 最大的接收缓冲队列长度
#define NET_PORT_TX_CHUNK        260    // 一次可发送的最大数据量
#define NET_PORT_TX_QUEUE_LEN    4      // 每个客户端自身最大的待发送缓冲队列个数

typedef enum {
    NET_PORT_OK = 0,
    NET_PORT_ERROR,
    NET_PORT_TIMEOUT,
    NET_PORT_BUSY,
    NET_PORT_INVALID_ARG
} net_port_result_t;

typedef struct {
    int fd;                             // 哪个客户端
    uint16_t len;                       // 本次收到的字节数
    uint8_t data[NET_PORT_RX_CHUNK];    // 原始字节，不解析报文
} net_port_rx_t;

/* 在 lwIP 初始化完成后调用；只创建队列，不创建任务。 */
net_port_result_t Net_Port_Init(uint16_t port);
/* 从 RX 队列取一段原始字节；timeout_ms = UINT32_MAX 表示永久等待。 */
net_port_result_t Net_Port_Recv(net_port_rx_t *event, uint32_t timeout_ms);
/* 向指定客户端排队发送原始字节；返回 BUSY 表示该客户端发送队列已满。 */
net_port_result_t Net_Port_Send(int fd, const uint8_t *data, uint16_t len);

/* 由 app 创建任务；内部负责 listen/accept/select/recv/send。 */
void NetServerTask(void *arg);

#endif
