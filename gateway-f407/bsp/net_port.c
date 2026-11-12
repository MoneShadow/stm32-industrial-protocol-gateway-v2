/* 原始 TCP 收发服务：socket / select / 接收队列 / 发送队列 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/errno.h"

#include "net_port.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NET_PORT_SELECT_TIMEOUT_MS  10
#define NET_PORT_RETRY_DELAY_MS     1000

/* 待发送数据类 */
typedef struct {
    uint16_t len;
    uint8_t data[NET_PORT_TX_CHUNK];
} net_port_tx_t;

/* 客户端类 */
typedef struct {
    int fd;
    QueueHandle_t tx_queue;
    uint8_t tx_buf[NET_PORT_TX_CHUNK];
    uint16_t tx_len;
    uint16_t tx_off;
} net_client_t;

static bool s_initialized;                              // 初始化标志
static bool s_server_started;                           // 服务开启标志
static uint16_t s_port;                                 // 指定的端口号
static QueueHandle_t s_rx_queue;                        // 接收缓冲队列
static SemaphoreHandle_t s_clients_mutex;               // 互斥锁
static net_client_t s_clients[NET_PORT_MAX_CLIENTS];    // 客户端对象数组

/* 
    设置非阻塞 socket 
    accept 没有新连接 → EAGAIN
    send 发送缓冲区满 → EAGAIN
    recv 数据读完了 → EAGAIN
*/
static int Net_SetNonBlocking(int fd)
{
    int flags = lwip_fcntl(fd, F_GETFL, 0); // 取出这个 fd 当前的标志位
    if (flags < 0) {
        return -1;
    }
    return lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK); // 把新的标志位写回去(非阻塞)
}

/* 重置/删除客户端 */
static void Net_ClientReset(net_client_t *client) {
    client->fd = -1;    // fd < 0 表示不存在该客户端/该客户端未启用
    client->tx_queue = NULL;
    client->tx_len = 0;
    client->tx_off = 0;
}

static void Net_ClientInitAll(void)
{
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        Net_ClientReset(&s_clients[i]);
    }
}

/* 关闭指定客户端(根据客户端数组索引) */
static void Net_ClientCloseByIndex(int index) {
    net_client_t *client = &s_clients[index];
    int fd;
    QueueHandle_t tx_queue;

    if (client->fd < 0) {
        return; // 客户端不存在
    }

    /* 加互斥锁 */
    xSemaphoreTake(s_clients_mutex, portMAX_DELAY);

    fd = client->fd;
    tx_queue = client->tx_queue;
    Net_ClientReset(client);    // 删除客户端

    /* 解互斥锁 */
    xSemaphoreGive(s_clients_mutex);

    if (fd >= 0) {
        (void)lwip_close(fd);   // 关闭该fd
    }
    if (tx_queue != NULL) {
        vQueueDelete(tx_queue); // 删除该客户端的tx接收缓冲队列
    }
}

/* 删除所有客户端 */
static void Net_ClientCloseAll(void) {
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        Net_ClientCloseByIndex(i);
    }
}

/* 初始化Client */
static int Net_ClientAdd(int fd) {
    int index = -1; // flag
    QueueHandle_t tx_queue;

    if (fd < 0 || fd >= FD_SETSIZE) {
        return -1;  // fd错误 
    }

    /* 给新的客户端创建自己的tx缓冲队列 */
    tx_queue = xQueueCreate(NET_PORT_TX_QUEUE_LEN, sizeof(net_port_tx_t));
    if (tx_queue == NULL) {
        return -1;
    }

    /* 加互斥锁 */
    xSemaphoreTake(s_clients_mutex, portMAX_DELAY);

    /* 遍历客户端数据 需要需要初始化的那个客户端 */
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        if (s_clients[i].fd < 0) {
            s_clients[i].fd = fd;
            s_clients[i].tx_queue = tx_queue;
            s_clients[i].tx_len = 0;
            s_clients[i].tx_off = 0;
            index = i;  // 更新flag 表示客户端添加成功
            break;
        }
    }

    /* 解互斥锁 */
    xSemaphoreGive(s_clients_mutex);

    if (index < 0) {
        /* 添加客户端失败 */
        vQueueDelete(tx_queue);
        return -1;
    }

    return index;   // 返回flag
}

/* 检查客户端存在的前提下 是否存在未发完的/没开始发的数据 */
static bool Net_ClientHasPendingTx(const net_client_t *client) {
    /* (1 && 1/0) == 1存在/0发完了 */
    return (client->fd >= 0) && (client->tx_len > client->tx_off);
}

/* 把已经排队的 TX 数据装进每个客户端的发送缓冲 */
static void Net_ClientLoadTx(net_client_t *client) {
    net_port_tx_t tx;

    if (client->fd < 0 || client->tx_queue == NULL) {
        return; // 表示该客户端不存在
    }
    if (Net_ClientHasPendingTx(client)) {
        return; // 当前消息还没发完，不能覆盖 tx_buf
    }

    /* 客户端存在并且存在未发完的/没开始发的数据 */
    if (xQueueReceive(client->tx_queue, &tx, 0) == pdPASS) {
        memcpy(client->tx_buf, tx.data, tx.len);    // 将待发送数据拷贝到客户端自己的发送缓冲区
        client->tx_len = tx.len;                    // 记录总长度
        client->tx_off = 0;                         // 记录已经发送了多少字节
    }
}

/* 实际发送数据的函数 */
static int Net_ClientFlushTx(net_client_t *client) {
    while (Net_ClientHasPendingTx(client)) {
        /* 该客户端存在待发送数据 返回成功写入发送队列的数据长度 */
        int sent = lwip_send(client->fd, client->tx_buf + client->tx_off, client->tx_len - client->tx_off, 0);

        if (sent > 0) {
            client->tx_off = (uint16_t)(client->tx_off + sent); // 更新已发送数据长度
            if (client->tx_off >= client->tx_len) {
                /* 数据发送完成 */
                client->tx_len = 0;
                client->tx_off = 0;
                Net_ClientLoadTx(client);
            }
        } else if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;   // TCP 发送缓冲区满了，现在发不出去
            }
            if (errno == EINTR) {
                continue;   // 被信号打断，重试
            }
            return -1;      // 连接出错
        } else {
            /* send() 返回 0，视为暂时没有进展，等下一次可写事件 */
            return 0;
        }
    }
    return 0;
}

/* new bind listen */
static int Net_ServerOpen(uint16_t port) {
    int fd;
    int opt = 1;
    struct sockaddr_in addr;

    /* new socket */
    fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || fd >= FD_SETSIZE) {
        if (fd >= 0) {
            (void)lwip_close(fd);
        }
        return -1;
    }

    /* socket属性设置 */
    (void)lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));                // 地址清零
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 任意IP地址
    addr.sin_port = htons(port);                // 指定端口

    /* bind */
    if (lwip_bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        (void)lwip_close(fd);
        return -1;
    }

    /* 转listen */
    if (lwip_listen(fd, NET_PORT_MAX_CLIENTS) < 0) {
        (void)lwip_close(fd);
        return -1;
    }

    /* 设置非阻塞 socket */
    if (Net_SetNonBlocking(fd) < 0) {
        (void)lwip_close(fd);
        return -1;
    }

    /*返回 listen_fd */
    return fd;
}

/* Net初始化 */
net_port_result_t Net_Port_Init(uint16_t port) {
    if (port == 0U) {
        return NET_PORT_INVALID_ARG;
    }

    if (s_initialized) {
        /* 重复初始化 */
        return NET_PORT_OK;
    }

    /* 创建接收缓冲队列 */
    s_rx_queue = xQueueCreate(NET_PORT_RX_QUEUE_LEN, sizeof(net_port_rx_t));
    if (s_rx_queue == NULL) {
        return NET_PORT_ERROR;
    }

    /* 创建互斥锁 */
    s_clients_mutex = xSemaphoreCreateMutex();
    if (s_clients_mutex == NULL) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
        return NET_PORT_ERROR;
    }

    /* 初始化所有client(不是添加逻辑) */
    Net_ClientInitAll();

    s_port = port;          // 指定端口
    s_initialized = true;   // 更新已初始化标示
    return NET_PORT_OK;
}

/* 对外暴露的接收API */
net_port_result_t Net_Port_Recv(net_port_rx_t *event, uint32_t timeout_ms) {
    TickType_t ticks;

    if (!s_initialized || event == NULL) {
        return NET_PORT_INVALID_ARG;
    }

    if (timeout_ms == UINT32_MAX) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout_ms);
    }

    /* 从接收缓冲队列中取出数据 */
    if (xQueueReceive(s_rx_queue, event, ticks) != pdPASS) {
        return NET_PORT_TIMEOUT;
    }

    return NET_PORT_OK;
}

/* 对外暴露的发送API */
net_port_result_t Net_Port_Send(int fd, const uint8_t *data, uint16_t len) {
    net_port_tx_t tx;
    net_port_result_t result = NET_PORT_ERROR;

    if (!s_initialized || data == NULL || len == 0U || len > NET_PORT_TX_CHUNK) {
        return NET_PORT_INVALID_ARG;
    }

    /* 加互斥锁 */
    xSemaphoreTake(s_clients_mutex, portMAX_DELAY);

    /* 遍历客户端数组 找到需要发送数据的那个客户端 */
    for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
        if (s_clients[i].fd == fd && s_clients[i].tx_queue != NULL) {
            tx.len = len;
            memcpy(tx.data, data, len);

            /* 将需要发送的数据添加到客户端自身的待发送数据缓冲队列 */
            if (xQueueSend(s_clients[i].tx_queue, &tx, 0) == pdPASS) {
                result = NET_PORT_OK;
            } else {
                result = NET_PORT_BUSY; // 队列满
            }
            break;
        }
    }

    /* 解互斥锁 */
    xSemaphoreGive(s_clients_mutex);
    return result;
}

/* 
    NET基础收发服务 
    流程：
        外层 for(;;)  // 重启服务器
        ↓
        创建 listener
        ↓
        内层 for(;;)  // 正常事件循环
        select
            ├─ 超时 → continue
            ├─ 新连接 → accept
            ├─ 有数据 → recv
            ├─ 可发送 → send
            ├─ EINTR → continue
            └─ 其他 select 错误 → break
        ↓
        关闭所有客户端 + listener
        ↓
        等 1 秒
        ↓
        重新创建 listener
*/
void NetServerTask(void *arg) {
    int listen_fd;      // 监听fd
    fd_set readfds;     // 可读清单
    fd_set writefds;    // 可写清单
    struct timeval tv;  // 超时

    (void)arg;

    if (!s_initialized) {
        /* 调用方必须先完成 lwIP 初始化和 Net_Port_Init()。 */
        vTaskDelay(pdMS_TO_TICKS(NET_PORT_RETRY_DELAY_MS));
        return;
    }

    /* 防止重复创建任务 */
    if (s_server_started) {
        vTaskDelete(NULL);
    }
    s_server_started = true;

    for (;;) {
        listen_fd = Net_ServerOpen(s_port);     // 直接到监听fd，函数内部负责 new bind listen
        if (listen_fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(NET_PORT_RETRY_DELAY_MS));
            continue;
        }

        /* 这个循环进入之后 除非发生错误 否则不会退出 */
        for (;;) {
            int maxfd = listen_fd;

            /* 把已经排队的 TX 数据装进每个客户端的发送缓冲。 */
            for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
                Net_ClientLoadTx(&s_clients[i]);
            }

            /* 初始化清空可读/写列表 */
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_SET(listen_fd, &readfds); // 添加listen_fd到可读列表

            /* 遍历客户端列表 添加已存在的客户端到可读列表 */
            for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
                net_client_t *client = &s_clients[i];

                if (client->fd < 0) {
                    /* 该客户端不存在 */
                    continue;
                }

                /* 加入可读列表 */
                FD_SET(client->fd, &readfds);
                /* 更新maxfd(最大fd的数值) */
                if (client->fd > maxfd) {
                    maxfd = client->fd;
                }

                /* 客户端存在的前提下 检查该客户端是否存在 待发的数据 */
                if (Net_ClientHasPendingTx(client)) {
                    /* 存在待发数据的客户端加入可写列表 */
                    FD_SET(client->fd, &writefds);
                }
            }

            tv.tv_sec = 0;
            tv.tv_usec = NET_PORT_SELECT_TIMEOUT_MS * 1000;

            /* 检查是否有可读/写的fd 返回值是需要处理的fd数量 每次select之后会更新可读/写列表 所以该列表需要在下次select之前更新 */
            int ready = lwip_select(maxfd + 1, &readfds, &writefds, NULL, &tv);

            /* 发生错误 唯一一处可以退出该循环的逻辑 */
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }

            /* 不存在可读/写fd事件 */
            if (ready == 0) {
                continue;
            }

            /* 新连接 */
            if (FD_ISSET(listen_fd, &readfds)) {
                for (;;) {
                    int fd = lwip_accept(listen_fd, NULL, NULL);

                    /* 连接出错 */
                    if (fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        if (errno == EINTR) {
                            continue;
                        }
                        break;
                    }

                    /* 设置非阻塞 socket 并添加客户端(初始化客户端) */
                    if (Net_SetNonBlocking(fd) < 0 || Net_ClientAdd(fd) < 0) {
                        (void)lwip_close(fd);
                    }
                }
            }

            /* 客户端接收 */
            for (int i = 0; i < NET_PORT_MAX_CLIENTS; i++) {
                net_client_t *client = &s_clients[i];
                int fd = client->fd;

                if (fd < 0) {
                    continue;   // 客户端不存在
                }

                if (FD_ISSET(fd, &readfds)) {
                    /* 客户端存在 并且存在可读事件(需要接收数据) */
                    for (;;) {  // 循环接收数据
                        uint8_t buf[NET_PORT_RX_CHUNK];
                        /*
                            recv > 0   收到数据
                            recv == 0  对端关闭
                            recv < 0   错误或暂时没数据 
                        */
                        int received = lwip_recv(fd, buf, sizeof(buf), 0);

                        if (received > 0) {
                            /* 收到数据 */
                            net_port_rx_t event;

                            event.fd = fd;
                            event.len = (uint16_t)received;
                            memcpy(event.data, buf, received);

                            /* 将接收到数据放入接收数据队列缓冲区 */
                            if (xQueueSend(s_rx_queue, &event, 0) != pdPASS) {
                                /* 消费端跟不上：断开该客户端，避免字节流错位。 */
                                Net_ClientCloseByIndex(i);
                                break;
                            }
                        } else if (received == 0) {
                            /* 对端发送了 FIN，正常关闭了连接 断开该客户端 */
                            Net_ClientCloseByIndex(i);
                            break;
                        } else {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;              // 数据读完了，但连接还在
                            }
                            if (errno == EINTR) {
                                continue;           // 被中断，重试
                            }
                            Net_ClientCloseByIndex(i);
                            break;                  // 真错误，关闭
                        }
                    }
                }

                if (client->fd >= 0 && FD_ISSET(fd, &writefds)) {
                    /* 客户端存在 且该客户端存在可写事件(存在待发送数据) */
                    if (Net_ClientFlushTx(client) < 0) {    // 发送数据
                        /* 连接出错 */
                        Net_ClientCloseByIndex(i);
                    }
                }
            }
        }

        /* 来到这里说明上面 select 返回了不可恢复错误 */
        Net_ClientCloseAll();              // 删除所有客户端
        (void)lwip_close(listen_fd);    // 关闭 listen_fd
        vTaskDelay(pdMS_TO_TICKS(NET_PORT_RETRY_DELAY_MS)); // 延时 1s 后重启服务
    }
}
