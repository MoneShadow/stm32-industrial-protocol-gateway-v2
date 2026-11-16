#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdint.h>

/* Modbus TCP ADU: MBAP 7 字节 + PDU，最大 260 字节。 */
#define MODBUS_TCP_MBAP_LEN  7U
#define MODBUS_TCP_PDU_MAX   253U
#define MODBUS_TCP_ADU_MAX   260U

typedef enum {
    MODBUS_TCP_OK = 0,
    MODBUS_TCP_NO_FRAME,
    MODBUS_TCP_INVALID_FRAME,
    MODBUS_TCP_OVERFLOW,
    MODBUS_TCP_INVALID_ARG
} modbus_tcp_result_t;

/*
 * 请求和响应共用同一个结构：
 * - Poll 时用于承载请求 PDU；
 * - Build 时用于承载响应 PDU。
 */
typedef struct {
    uint16_t transaction_id;
    uint8_t  unit_id;
    uint8_t  pdu[MODBUS_TCP_PDU_MAX];
    uint16_t pdu_len;
} modbus_tcp_message_t;

/* 一条已经组好的、可直接交给 Net_Port_Send 的完整 ADU。 */
typedef struct {
    uint8_t  data[MODBUS_TCP_ADU_MAX];
    uint16_t len;
} modbus_tcp_adu_t;

typedef struct {
    uint8_t  buf[MODBUS_TCP_ADU_MAX];
    uint16_t len;
} modbus_tcp_conn_t;

/* 每个 TCP 连接初始化一次，清空接收缓冲。 */
void Modbus_Tcp_Init(modbus_tcp_conn_t *conn);

/* 把 net_port 收到的原始字节追加到连接缓冲。 */
modbus_tcp_result_t Modbus_Tcp_Feed(modbus_tcp_conn_t *conn, const uint8_t *data, uint16_t len);

/* 尝试从缓冲中取出一个完整请求；没有完整帧返回 NO_FRAME。 */
modbus_tcp_result_t Modbus_Tcp_Poll(modbus_tcp_conn_t *conn, modbus_tcp_message_t *message);

/* 用带 TID/Unit/PDU 的响应结构体生成完整响应 ADU。 */
modbus_tcp_result_t Modbus_Tcp_Build(const modbus_tcp_message_t *message, modbus_tcp_adu_t *adu);

#endif
