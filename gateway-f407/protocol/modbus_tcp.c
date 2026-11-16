/* Modbus TCP：TCP 字节流 → 完整 MBAP/PDU；不解析 0x03/0x06 语义。 */
#include "modbus_tcp.h"

#include <stddef.h>
#include <string.h>

/* MBAP 前 6 字节：TID(2) + Protocol(2) + Length(2) */
#define MODBUS_TCP_HEADER_LEN  6U
#define MODBUS_TCP_UNIT_LEN    1U
#define MODBUS_TCP_LENGTH_MIN  2U   /* Unit ID + 至少 1 字节 PDU */
#define MODBUS_TCP_LENGTH_MAX  254U /* Unit ID + 253 字节 PDU */

/* 
    ModbusTCP MBAP头均为大端序(除开Unit ID 占用1字节) 该函数用于读取MBAP头的各个字段
    Transaction ID   2 字节
    Protocol ID      2 字节，必须为 0
    Length           2 字节，Unit ID + PDU 长度
    Unit ID          1 字节
*/
static uint16_t Modbus_Tcp_ReadU16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/*  */
static void Modbus_Tcp_WriteU16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFFU);
}

/* 初始化 modbus_tcp_conn_t */
void Modbus_Tcp_Init(modbus_tcp_conn_t *conn) {
    if (conn == NULL) {
        return;
    }

    memset(conn, 0, sizeof(*conn));
}

/* 把一次 recv 的原始字节追加到该客户端的解析缓冲 modbus_tcp_conn_t */
modbus_tcp_result_t Modbus_Tcp_Feed(modbus_tcp_conn_t *conn, const uint8_t *data, uint16_t len) {
    if (conn == NULL || data == NULL) {
        return MODBUS_TCP_INVALID_ARG;
    }

    if (len == 0U) {
        return MODBUS_TCP_OK;
    }

    if (len > (uint16_t)(MODBUS_TCP_ADU_MAX - conn->len)) {
        /* 缓冲里已经不是一个完整的最大 ADU，无法可靠继续拼帧。 */
        conn->len = 0;
        return MODBUS_TCP_OVERFLOW;
    }

    memcpy(&conn->buf[conn->len], data, len);
    conn->len = (uint16_t)(conn->len + len);

    return MODBUS_TCP_OK;
}

/* 把 modbus_tcp_conn_t 中的数据解析成完整的一帧modbustcp报文(一个Feed可能含有多个报文) */
modbus_tcp_result_t Modbus_Tcp_Poll(modbus_tcp_conn_t *conn,
                                    modbus_tcp_message_t *message) {
    uint16_t protocol_id;
    uint16_t length;
    uint16_t total_len;

    if (conn == NULL || message == NULL) {
        return MODBUS_TCP_INVALID_ARG;
    }

    /* conn里面的数据长度连个MBAP头都不够 还解析啥了 */
    if (conn->len < MODBUS_TCP_HEADER_LEN) {
        return MODBUS_TCP_NO_FRAME;
    }

    /* 从conn里获取MBAP头 */
    protocol_id = Modbus_Tcp_ReadU16(&conn->buf[2]);
    length = Modbus_Tcp_ReadU16(&conn->buf[4]);         // unitidlength + pdulength

    if (protocol_id != 0U || length < MODBUS_TCP_LENGTH_MIN || length > MODBUS_TCP_LENGTH_MAX) {
        conn->len = 0;
        return MODBUS_TCP_INVALID_FRAME;
    }

    total_len = (uint16_t)(MODBUS_TCP_HEADER_LEN + length); // 一帧完整的modbustcp报文的长度
    if (total_len > MODBUS_TCP_ADU_MAX) {
        /* 超过了最大长度 说明字节流出问题了 */
        conn->len = 0;
        return MODBUS_TCP_INVALID_FRAME;
    }

    /* 如果客户端的conn中接收到的数据长度小于 解析+拼头得到的一帧完整报文长度 说明conn中的数据是不完整的一帧报文，通知上层继续接收 不代表出错 */
    if (conn->len < total_len) {
        return MODBUS_TCP_NO_FRAME;
    }

    message->transaction_id = Modbus_Tcp_ReadU16(&conn->buf[0]);
    message->unit_id = conn->buf[6];
    message->pdu_len = (uint16_t)(length - MODBUS_TCP_UNIT_LEN);

    memcpy(message->pdu,
           &conn->buf[MODBUS_TCP_MBAP_LEN],
           message->pdu_len);

    if (conn->len > total_len) {
        /* 把已经解析出来的第一帧从缓冲区头部移走，并把后面的剩余字节搬到开头 */
        memmove(conn->buf, &conn->buf[total_len], (size_t)(conn->len - total_len));
    }
    conn->len = (uint16_t)(conn->len - total_len);  // 更新待解析数据长度

    return MODBUS_TCP_OK;
}

/* 给响应PDU添加MBAP头 构造成完整可直接发送的ADU 其实原本传入的message已经完整，但是这个无法直接发送 */
modbus_tcp_result_t Modbus_Tcp_Build(const modbus_tcp_message_t *message, modbus_tcp_adu_t *adu) {
    uint16_t total_len;
    uint16_t length_field;

    if (message == NULL || adu == NULL) {
        return MODBUS_TCP_INVALID_ARG;
    }

    if (message->pdu_len == 0U || message->pdu_len > MODBUS_TCP_PDU_MAX) {
        return MODBUS_TCP_INVALID_ARG;
    }

    total_len = (uint16_t)(MODBUS_TCP_MBAP_LEN + message->pdu_len);
    if (total_len > MODBUS_TCP_ADU_MAX) {
        return MODBUS_TCP_INVALID_ARG;
    }

    length_field = (uint16_t)(MODBUS_TCP_UNIT_LEN + message->pdu_len);

    Modbus_Tcp_WriteU16(&adu->data[0], message->transaction_id);
    Modbus_Tcp_WriteU16(&adu->data[2], 0U);          /* Protocol ID */
    Modbus_Tcp_WriteU16(&adu->data[4], length_field);
    adu->data[6] = message->unit_id;
    memcpy(&adu->data[MODBUS_TCP_MBAP_LEN], message->pdu, message->pdu_len);

    adu->len = total_len;
    return MODBUS_TCP_OK;
}
