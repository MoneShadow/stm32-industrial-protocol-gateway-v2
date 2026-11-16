/* Modbus PDU：纯语义解析/组装；不访问设备模型、不发送 CAN。 */
#include "modbus_pdu.h"

#include <stddef.h>

/* 读/写大端序数据 */
static uint16_t Modbus_Pdu_ReadU16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void Modbus_Pdu_WriteU16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFFU);
}

/*
    将tcp之后得到的一帧完整的modbustcp报文进行语义解析
*/
modbus_pdu_result_t Modbus_Pdu_Parse(const uint8_t *pdu, uint16_t pdu_len, modbus_pdu_request_t *request) {
    if (pdu == NULL || request == NULL) {
        return MODBUS_PDU_INVALID_ARG;
    }

    request->function = 0U;
    request->address = 0U;
    request->quantity = 0U;
    request->value = 0U;

    if (pdu_len == 0U) {
        return MODBUS_PDU_INVALID_LENGTH;
    }

    request->function = pdu[0];

    switch (pdu[0]) {
    case MODBUS_PDU_FUNC_READ_HOLDING:
        if (pdu_len != 5U) {
            return MODBUS_PDU_INVALID_LENGTH;
        }

        request->address = Modbus_Pdu_ReadU16(&pdu[1]);
        request->quantity = Modbus_Pdu_ReadU16(&pdu[3]);

        if (request->quantity == 0U || request->quantity > MODBUS_PDU_READABLE_QTY) {
            return MODBUS_PDU_INVALID_VALUE;
        }

        if ((uint32_t)request->address + (uint32_t)request->quantity >
            0x10000UL) {
            return MODBUS_PDU_INVALID_ADDRESS;
        }

        return MODBUS_PDU_OK;

    case MODBUS_PDU_FUNC_WRITE_SINGLE:
        if (pdu_len != 5U) {
            return MODBUS_PDU_INVALID_LENGTH;
        }

        request->address = Modbus_Pdu_ReadU16(&pdu[1]);
        request->value = Modbus_Pdu_ReadU16(&pdu[3]);
        return MODBUS_PDU_OK;

    default:
        return MODBUS_PDU_UNSUPPORTED_FUNCTION;
    }
}

modbus_pdu_result_t Modbus_Pdu_BuildReadResponse(const uint16_t *values, uint16_t quantity, modbus_pdu_t *response) {
    uint16_t response_len;

    if (values == NULL || response == NULL) {
        return MODBUS_PDU_INVALID_ARG;
    }

    if (quantity == 0U || quantity > MODBUS_PDU_READABLE_QTY) {
        response->len = 0U;
        return MODBUS_PDU_INVALID_VALUE;
    }

    response_len = (uint16_t)(2U + (quantity * 2U));

    response->data[0] = MODBUS_PDU_FUNC_READ_HOLDING;   // 功能码
    response->data[1] = (uint8_t)(quantity * 2U);       // 数据字节长度

    /* 写入数据 */
    for (uint16_t i = 0; i < quantity; i++) {
        Modbus_Pdu_WriteU16(&response->data[2U + (i * 2U)], values[i]);
    }

    response->len = response_len;                       // 记录回应PDU总长度
    return MODBUS_PDU_OK;
}

modbus_pdu_result_t Modbus_Pdu_BuildWriteEcho(const modbus_pdu_request_t *request, modbus_pdu_t *response) {
    /* 写寄存器回显其实就是原样回显 */
    if (request == NULL || response == NULL) {
        return MODBUS_PDU_INVALID_ARG;
    }

    if (request->function != MODBUS_PDU_FUNC_WRITE_SINGLE) {
        response->len = 0U;
        return MODBUS_PDU_INVALID_ARG;
    }

    response->data[0] = MODBUS_PDU_FUNC_WRITE_SINGLE;
    Modbus_Pdu_WriteU16(&response->data[1], request->address);
    Modbus_Pdu_WriteU16(&response->data[3], request->value);
    response->len = 5U;

    return MODBUS_PDU_OK;
}

modbus_pdu_result_t Modbus_Pdu_BuildException(uint8_t function, uint8_t exception, modbus_pdu_t *response) {
    if (response == NULL) {
        return MODBUS_PDU_INVALID_ARG;
    }

    response->data[0] = (uint8_t)(function | 0x80U);
    response->data[1] = exception;
    response->len = 2U;

    return MODBUS_PDU_OK;
}
