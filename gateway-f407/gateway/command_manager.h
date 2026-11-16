#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

typedef struct {
    uint8_t seq;
    uint8_t command;
} command_t;

extern QueueHandle_t g_command_queue;

/* 创建已提交的命令队列；在 Gateway_Init() 中调用 */
void Command_Init(void);

/* 由command_manager维护的全局seq生成器 */
uint8_t Command_Generate_Seq(uint16_t address);
uint8_t Command_Input_Queue(void);
BaseType_t Command_Out_Queue(void);
void Command_Recover_Seq(void);

/* 发送命令任务 */
void CommandTask(void *arg);

#endif
