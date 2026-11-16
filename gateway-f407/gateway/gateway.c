/* Gateway：只负责收集、翻译、投递；命令执行在 command_manager.c。 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "gateway.h"
#include "gateway_internal.h"
#include "gateway_translate.h"
#include "net_port.h"
#include "modbus_tcp.h"
#include "modbus_pdu.h"
#include "device_model.h"
#include "command_manager.h"
#include "can_frame.h"
#include "can_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define GATEWAY_MODBUS_QUEUE_LEN    8
#define GATEWAY_CAN_QUEUE_LEN       16
#define GATEWAY_TRANSLATE_QUEUE_LEN 8
#define GATEWAY_WAIT_MS             10U

static QueueHandle_t s_modbus_queue;
static QueueHandle_t s_can_queue;
QueueHandle_t g_translate_queue;
static QueueSetHandle_t s_queue_set;

BaseType_t Gateway_PostModbus(const gateway_modbus_msg_t *msg) {
    if (s_modbus_queue == NULL || msg == NULL) {
        return pdFAIL;
    }
    return xQueueSend(s_modbus_queue, msg, 0);
}

BaseType_t Gateway_PostCan(const gateway_can_msg_t *msg) {
    if (s_can_queue == NULL || msg == NULL) {
        return pdFAIL;
    }
    return xQueueSend(s_can_queue, msg, 0);
}

BaseType_t Gateway_PostTranslate(const gateway_translate_t *msg) {
    if (g_translate_queue == NULL || msg == NULL) {
        return pdFAIL;
    }
    return xQueueSend(g_translate_queue, msg, 0);
}

void Gateway_Init(void) {
    s_modbus_queue = xQueueCreate(GATEWAY_MODBUS_QUEUE_LEN, sizeof(gateway_modbus_msg_t));
    s_can_queue = xQueueCreate(GATEWAY_CAN_QUEUE_LEN, sizeof(gateway_can_msg_t));
    g_translate_queue = xQueueCreate(GATEWAY_TRANSLATE_QUEUE_LEN, sizeof(gateway_translate_t));
    s_queue_set = xQueueCreateSet(GATEWAY_MODBUS_QUEUE_LEN + GATEWAY_CAN_QUEUE_LEN);

    configASSERT(s_modbus_queue != NULL);
    configASSERT(s_can_queue != NULL);
    configASSERT(g_translate_queue != NULL);
    configASSERT(s_queue_set != NULL);

    configASSERT(xQueueAddToSet(s_modbus_queue, s_queue_set) == pdPASS);
    configASSERT(xQueueAddToSet(s_can_queue, s_queue_set) == pdPASS);

    Device_Model_Init();
    Command_Init();
}

/* ModbusTCP 转 Can 返回值：0成功 1编码失败 2入队失败 3存在在途命令 */
static uint8_t Gateway_ModbustoCan(gateway_modbus_msg_t *message, gateway_translate_t *frame) {
    uint8_t flag = 0, readflag = 0;
    switch (message->request.function) {
        case MODBUS_PDU_FUNC_READ_HOLDING:  // 读保持寄存器
            /* 保留两份 相同意思不同翻译的信息 */
            memcpy(&frame->modbus_msg, message, sizeof(gateway_modbus_msg_t));
            readflag = 1;
            flag = Can_App_FetchState_Code(message->request.address, message->request.quantity, Command_Generate_Seq(0), &frame->can_frame);
            break;
        case MODBUS_PDU_FUNC_WRITE_SINGLE:  // 写单个保存寄存器
            /* 如果发现还有命令没有处理完成 自动丢弃本次命令 此处只需要判断队列中是否有数据即可无须查看命令状态 因为非在途的命令状态的命令会被及时清除 */
            if (uxQueueMessagesWaiting(g_command_queue) > 0) {
                return 3;
            }
            /* 没有在途命令 */
            if (message->request.address == DEVICE_REG_FAN_RPM) {
                /* 保留两份 相同意思不同翻译的信息 */ 
                memcpy(&frame->modbus_msg, message, sizeof(gateway_modbus_msg_t));
                flag = Can_App_Fan_Code(message->request.value, Command_Generate_Seq(message->request.address), &frame->can_frame);
            }
            else if (message->request.address == DEVICE_REG_LED_STATE) {
                /* 保留两份 相同意思不同翻译的信息 */
                memcpy(&frame->modbus_msg, message, sizeof(gateway_modbus_msg_t));
                /* 此处传入led命令编码的 value 本体是16位的 但是这里led的状态是 可以是非零和零的逻辑(下位机规定一下就好) 而且上位机也不会允许恶意指令(测试情况除外) */
                flag = Can_App_LED_Code(message->request.value, Command_Generate_Seq(message->request.address), &frame->can_frame);
            }
            else {
                flag = 1;
            }
            break;
        default:
            flag = 1;
            break;
    }
    if (!flag && !readflag) {
        if (Command_Input_Queue()) {  // 生成的命令入队
            /* 入队失败 */
            flag = 2;
        }
    }
    else {
        if (!readflag) {
            Command_Recover_Seq();  // 命令生成失败 恢复序号
        }
    }
    return flag;    // 0成功 1编码失败 2入队失败 3存在在途命令
}

/*
    Can 转 ModbusTCP 有一些特殊
    比如 0x101状态 0x201心跳 0x401ACK 均不属于下位机向上位机发送的命令 因为modbus中下位机不允许主动向上位机发送信
    所以这些命令其实是不需要转译的 需要转译的命令其实是 PC下发读寄存器命令 然后下位机回应的那条Can命令
    但是那一条Can命令下位机不会发送 因为当前项目规定 网关可以中转状态 所以PC下的读寄存器命令 其实并不经过下位机
    所以这里的翻译其实只是装装样子 实际直接根据Can命令执行相对应操作即可 不再需要转移后经过 commandmanager 发送
*/
static void Gateway_CanToModbus(gateway_can_msg_t *message) {
    uint32_t now_ms = 0;
    switch (message->type) {
        case GATEWAY_CAN_STATUS:
            Device_Model_UpdateStatus(&message->data.status);
            break;
        case GATEWAY_CAN_HEARTBEAT:
            now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            Device_Model_UpdateHeartbeat(&message->data.heart, now_ms);
            break;
        case GATEWAY_CAN_ACK:
            now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            Device_Model_UpdateACK(&message->data.ack, now_ms);
            break;
        default:
            break;
    }
}

/* 发送回显 不可直接由任务调用 */
static void Gateway_SendPdu(const gateway_modbus_msg_t *msg, const modbus_pdu_t *pdu) {
    modbus_tcp_message_t response = {0};
    modbus_tcp_adu_t adu;
    if (pdu == NULL || pdu->len > MODBUS_TCP_PDU_MAX) {
        return;
    }
    response.transaction_id = msg->transaction_id;
    response.unit_id = msg->unit_id;
    response.pdu_len = pdu->len;
    memcpy(response.pdu, pdu->data, pdu->len);
    if (Modbus_Tcp_Build(&response, &adu) == MODBUS_TCP_OK) {
        (void)Net_Port_Send(msg->fd, msg->conn_id, adu.data, adu.len);
    }
}

/* 异常回显 */
static void Gateway_SendException(const gateway_modbus_msg_t *msg, uint8_t function, uint8_t exception) {
    modbus_pdu_t response_pdu = {0};
    if (function == 0U) {
        return;
    }
    if (Modbus_Pdu_BuildException(function, exception, &response_pdu) == MODBUS_PDU_OK) {
        Gateway_SendPdu(msg, &response_pdu);
    }
}

/* mdobus翻译结果 转 Modbus异常码 */
static uint8_t Gateway_ModbusPrase_to_ModbusState(modbus_pdu_result_t parse_result) {
    switch (parse_result) {
        case MODBUS_PDU_UNSUPPORTED_FUNCTION:
            return MODBUS_PDU_EXC_ILLEGAL_FUNCTION;
        case MODBUS_PDU_INVALID_LENGTH:
        case MODBUS_PDU_INVALID_ADDRESS:
            return MODBUS_PDU_EXC_ILLEGAL_ADDRESS;
        case MODBUS_PDU_INVALID_VALUE:
        case MODBUS_PDU_INVALID_ARG:
            return MODBUS_PDU_EXC_ILLEGAL_VALUE;
        default:
            return MODBUS_PDU_EXC_DEVICE_FAILURE;
    }
}

void GatewayTask(void *arg) {
    (void)arg;

    for (;;) {
        QueueSetMemberHandle_t member = xQueueSelectFromSet(s_queue_set, pdMS_TO_TICKS(GATEWAY_WAIT_MS));

        if (member == s_modbus_queue) {
            gateway_modbus_msg_t message;
            while (xQueueReceive(s_modbus_queue, &message, pdMS_TO_TICKS(10)) == pdPASS) {
                if (message.parse_result != MODBUS_PDU_OK) {
                    /* 翻译阶段出现失败 根据翻译失败类型发送对应回显 */
                    Gateway_SendException(&message, message.request.function, Gateway_ModbusPrase_to_ModbusState(message.parse_result));
                    continue;   // 跳过本次命令
                }
                gateway_translate_t translate_can_frame = {0};
                /* 调用modbus转can */
                if (Gateway_ModbustoCan(&message, &translate_can_frame)) {
                    /* 转译失败 跳过本次命令 并发送异常回显(所有错误暂时都归结为入队失败 因为所有错误的共性就是没有入队) */
                    Gateway_SendException(&message, message.request.function, MODBUS_PDU_EXC_SERVER_BUSY);
                    continue;
                }
                if (Gateway_PostTranslate(&translate_can_frame) != pdPASS) {
                    /*
                        转译队列满 这个问题很严重 来到这里 本次命令已经入队 已经入队说明什么 说明没有在途命令影响本次命令入队
                        在没有在途命令的情况下 转译队列居然是满的？这说明什么 说明取该队列的调用方发生了错误 所有命令都被记录成超时
                        所以如果真的来到这里 说明发生了不可预测的错误 所以此处需要进行断言 之后可以搭配看门狗重启服务

                        忘记考虑读寄存器命令了 读寄存器命令不往已提交命令队列中发送 所以这里有可能存在转译队列中全是读寄存器命令的情况
                        这种情况是有可能的 因为读寄存器命令没有明确限流 所以我应该提升一下转译队列的长度
                        所以 如果满了话 就丢弃本次命令并异常回显吧 注意把已经提交的命令手动出队丢弃掉
                    */
                    /* 这里的入队失败 和上面的入队失败不是同一个队列 这里没有做区分 所以就标注一下 */
                    if (message.request.function == MODBUS_PDU_FUNC_WRITE_SINGLE) {
                        /* 如果是读命令入队失败的话 直接回显就好 万万不可取出可能正在在途的写命令 */
                        if (Command_Out_Queue() != pdPASS) {
                            /* 丢弃失败？会有这种情况产生吗？明明命令才刚刚提交到队列中！暂时不处理 */
                        }
                    }
                    Gateway_SendException(&message, message.request.function, MODBUS_PDU_EXC_SERVER_BUSY);
                }
            }
        }

        if (member == s_can_queue) {
            gateway_can_msg_t message;
            while (xQueueReceive(s_can_queue, &message, pdMS_TO_TICKS(10)) == pdPASS) {
                /* 这里看似调用了转译函数 实际上并非转译 所以也不存在推送 */
                Gateway_CanToModbus(&message);
            }
        }

        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        (void)Device_Model_HeartbeatTick(now_ms);
        (void)Device_Model_ACKTick(now_ms);
    }
}
