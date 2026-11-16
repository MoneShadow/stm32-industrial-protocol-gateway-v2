#ifndef GATEWAY_H
#define GATEWAY_H

/* 初始化 Gateway 队列/queue set、device_model 和 command_manager。 */
void Gateway_Init(void);

/* Modbus TCP 接收/PDU 解析任务。 */
void Gateway_ModbusRxTask(void *arg);

/* CAN 接收/can_app 解码任务。 */
void Gateway_CanRxTask(void *arg);

/* 消费 Modbus/CAN 两个语义队列，完成翻译和投递。 */
void GatewayTask(void *arg);

#endif
