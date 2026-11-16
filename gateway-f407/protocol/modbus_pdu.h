#ifndef MODBUS_PDU_H
#define MODBUS_PDU_H

#include <stdint.h>

/* Modbus PDU 最大长度：功能码 1 + 数据 最多 252。 */
#define MODBUS_PDU_MAX             253U
#define MODBUS_PDU_MAX_READ_QTY    125U

/* 真实的可读寄存器数量 */
#define MODBUS_PDU_READABLE_QTY    7U

/* 功能码 */
#define MODBUS_PDU_FUNC_READ_HOLDING 0x03U
#define MODBUS_PDU_FUNC_WRITE_SINGLE 0x06U

/* 异常码 */
#define MODBUS_PDU_EXC_ILLEGAL_FUNCTION 0x01U
#define MODBUS_PDU_EXC_ILLEGAL_ADDRESS  0x02U
#define MODBUS_PDU_EXC_ILLEGAL_VALUE    0x03U
#define MODBUS_PDU_EXC_DEVICE_FAILURE   0x04U
#define MODBUS_PDU_EXC_SERVER_BUSY     0x06U

/* PDU 层返回码，不是 Modbus 异常码。 */
typedef enum {
    MODBUS_PDU_OK = 0,
    MODBUS_PDU_UNSUPPORTED_FUNCTION,
    MODBUS_PDU_INVALID_LENGTH,
    MODBUS_PDU_INVALID_ADDRESS,
    MODBUS_PDU_INVALID_VALUE,
    MODBUS_PDU_INVALID_ARG
} modbus_pdu_result_t;

/*
 * 语义请求：
 * - 0x03 使用 address + quantity；
 * - 0x06 使用 address + value。
 */
typedef struct {
    uint8_t function;
    uint16_t address;
    uint16_t quantity;
    uint16_t value;
} modbus_pdu_request_t;

/* 响应 PDU 容器：data/len 由 Build API 填充。 */
typedef struct {
    uint8_t data[MODBUS_PDU_MAX];
    uint16_t len;
} modbus_pdu_t;

/* PDU 字节 → 语义请求；不执行任何寄存器操作。 */
modbus_pdu_result_t Modbus_Pdu_Parse(const uint8_t *pdu, uint16_t pdu_len, modbus_pdu_request_t *request);

/* 0x03 响应 PDU：功能码 + 字节数 + 寄存器值。 */
modbus_pdu_result_t Modbus_Pdu_BuildReadResponse(const uint16_t *values, uint16_t quantity, modbus_pdu_t *response);

/* 0x06 响应 PDU：原样回显地址和值。 */
modbus_pdu_result_t Modbus_Pdu_BuildWriteEcho(const modbus_pdu_request_t *request, modbus_pdu_t *response);

/* 异常响应 PDU：功能码 | 0x80 + 异常码。 */
modbus_pdu_result_t Modbus_Pdu_BuildException(uint8_t function, uint8_t exception, modbus_pdu_t *response);

#endif
