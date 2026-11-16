/* Command Manager：翻译队列消费 + 命令状态机 + CAN 发送 + Modbus 回显。 */
#include "command_manager.h"

#include "FreeRTOS.h"
#include "task.h"

#include "device_model.h"
#include "gateway_translate.h"
#include "can_port.h"
#include "net_port.h"
#include "modbus_tcp.h"
#include "modbus_pdu.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define COMMAND_TIMEOUT_MS              1000U
#define COMMAND_TASK_WAIT_MS            10U
#define COMMAND_SET_FAN                 01U
#define COMMAND_SET_LED                 02U
#define COMMAND_SAVECOMMAND_QUEUE_LEN   1

/* 
    需要维护一个状态机
    用来保存上次的命令 以及序号
    如果只提供序号的话 ACK来了之后无法确认命令 也无法根据序号确认是哪个命令执行成功/失败
    可以创建一个结构体 
*/

QueueHandle_t g_command_queue;
static uint8_t main_seq = 0;
uint8_t last_seq = 0;
command_t command = {0};

void Command_Init(void) {
    g_command_queue = xQueueCreate(COMMAND_SAVECOMMAND_QUEUE_LEN, sizeof(command_t));
    while (g_command_queue == NULL) {
        /* 初始化失败 强行运行下去 也会导致后面的程序出现问题 所以没有必要继续运行 应该停下来 检查问题 所以此处死循环 */
    }
}

uint8_t Command_Generate_Seq(uint16_t address) {
    last_seq = main_seq;
    if (address == DEVICE_REG_FAN_RPM) {
        command.command = COMMAND_SET_FAN;
    }
    else if (address == DEVICE_REG_LED_STATE){
        command.command = COMMAND_SET_LED;
    }
    else {
        /* 读寄存器命令暂时不给seq 命令不下发 seq暂时没用 */
        // command.seq = main_seq++;
        return 0;
    }
    command.seq = main_seq++;
    return command.seq;
}

/* 和 Command_Generate_Seq 配合 出现 Command_Generate_Seq 必定出现 Command_Input_Queue/Command_Recover_Seq */
uint8_t Command_Input_Queue(void) {
    if (xQueueSend(g_command_queue, &command, pdMS_TO_TICKS(10)) != pdPASS) {
        /* 入队失败 说明上一条命令还没有接收到ACK或者还未超时 新命令丢弃 */
        main_seq = last_seq;    // 恢复seq
        return 1;
    }
    return 0;
}

/* 命令出队丢弃 */
BaseType_t Command_Out_Queue(void) {
    command_t abandon_command = {0};
    BaseType_t result = xQueueReceive(g_command_queue, &abandon_command, pdMS_TO_TICKS(10));
    if (result == pdPASS) {
        Command_Recover_Seq();
    }
    return result;
}

void Command_Recover_Seq(void) {
    command.seq = last_seq;
    main_seq = last_seq;
}

/* 发送回显 不可直接由任务调用 */
static void Command_SendPdu(const gateway_translate_t *msg, const modbus_pdu_t *pdu) {
    modbus_tcp_message_t response = {0};
    modbus_tcp_adu_t adu;
    if (pdu == NULL || pdu->len > MODBUS_TCP_PDU_MAX) {
        return;
    }
    response.transaction_id = msg->modbus_msg.transaction_id;
    response.unit_id = msg->modbus_msg.unit_id;
    response.pdu_len = pdu->len;
    memcpy(response.pdu, pdu->data, pdu->len);
    if (Modbus_Tcp_Build(&response, &adu) == MODBUS_TCP_OK) {
        (void)Net_Port_Send(msg->modbus_msg.fd, msg->modbus_msg.conn_id, adu.data, adu.len);
    }
}

/* 异常回显 */
static void Command_SendException(const gateway_translate_t *msg, uint8_t function, uint8_t exception) {
    modbus_pdu_t response_pdu = {0};
    if (function == 0U) {
        return;
    }
    if (Modbus_Pdu_BuildException(function, exception, &response_pdu) == MODBUS_PDU_OK) {
        Command_SendPdu(msg, &response_pdu);
    }
}

/* 写回显 */
static void Command_SendWriteEcho(const gateway_translate_t *msg) {
    modbus_pdu_t response_pdu = {0};
    if (Modbus_Pdu_BuildWriteEcho(&msg->modbus_msg.request, &response_pdu) == MODBUS_PDU_OK) {
        Command_SendPdu(msg, &response_pdu);
    }
}
/* 读回显 */
static void Command_SendReadEcho(const gateway_translate_t *msg, const uint16_t *value) {
    modbus_pdu_t response_pdu = {0};
    if (Modbus_Pdu_BuildReadResponse(value ,msg->modbus_msg.request.quantity, &response_pdu) == MODBUS_PDU_OK) {
        Command_SendPdu(msg, &response_pdu);
    }
}

/* 结果 -> 状态 */
static uint8_t Command_Device_to_ModbusState(device_model_result_t result) {
    switch (result) {
        case DEVICE_MODEL_BAD_ADDRESS:
            return MODBUS_PDU_EXC_ILLEGAL_ADDRESS;
        case DEVICE_MODEL_BAD_VALUE:
            return MODBUS_PDU_EXC_ILLEGAL_VALUE;
        default:
            return MODBUS_PDU_EXC_DEVICE_FAILURE;
    }
}

/* 结果 -> 状态 */
static uint8_t Command_CanSend_to_ModbusState(can_port_result_t result) {
    switch (result) {
        case CAN_PORT_INVALID_ARG:
            return MODBUS_PDU_EXC_ILLEGAL_VALUE;
        case CAN_PORT_BUSY:
        default:
            return MODBUS_PDU_EXC_DEVICE_FAILURE;
    }
}

void CommandTask(void *arg) {
    (void)arg;
    for (;;) {
        gateway_translate_t msg = {0};
        xQueueReceive(g_translate_queue, &msg, pdMS_TO_TICKS(portMAX_DELAY));
        switch (msg.can_frame.id) {
            case 0x501: {   // 读寄存器
                // 这里待会调用 device_model 实现的读寄存器
                uint16_t value[7] = {0};
                device_model_result_t result = 0;
                if ((result = Device_Model_Read(&msg.can_frame, value)) != DEVICE_MODEL_OK) {
                    /* 发送失败 异常回显 */
                    Command_SendException(&msg, msg.modbus_msg.request.function, Command_Device_to_ModbusState(result));
                }
                else {
                    /* 发送成功 读回显 */
                    Command_SendReadEcho(&msg, value);
                }
                break;
            }
            case 0x301: {   // 写寄存器
                can_port_result_t result = 0;
                if ((result = Can_Port_send(&msg.can_frame)) != CAN_PORT_OK) {
                    /* 发送失败 异常回显 */
                    gateway_translate_t sendfaild_command = {0};
                    xQueueReceive(g_command_queue, &sendfaild_command, pdMS_TO_TICKS(100));
                    Device_Model_Cmd_State(COMMAND_STATE_FAILD);
                    Command_SendException(&msg, MODBUS_PDU_FUNC_WRITE_SINGLE, Command_CanSend_to_ModbusState(result));
                }
                else {
                    /* 发送成功 不代表执行成功 只能表示命令在途 而且当前命令载体是Can 如果是多从机的话还需要经过仲裁 */
                    Device_Model_Cmd_State(COMMAND_STATE_RUNNING);
                    /* modbus回显 */
                    Command_SendWriteEcho(&msg);
                }
                break;
            }
            default:
                break;
        }
    }
}
