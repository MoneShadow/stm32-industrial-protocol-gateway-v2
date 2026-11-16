#ifndef DEVICE_MODEL_H
#define DEVICE_MODEL_H

#include "FreeRTOS.h"
#include "semphr.h"

#include <stdbool.h>
#include <stdint.h>

#include "can_protocol.h"
#include "can_frame.h"

/* Modbus 保持寄存器地址 */
#define DEVICE_REG_TEMPERATURE      0x0000U
#define DEVICE_REG_LIGHT            0x0001U
#define DEVICE_REG_FAN_RPM          0x0002U
#define DEVICE_REG_LED_STATE        0x0003U
#define DEVICE_REG_CMD_RESULT       0x0004U
#define DEVICE_REG_ONLINE           0x0005U
#define DEVICE_REG_PROTOCOL_VERSION 0x0006U

extern SemaphoreHandle_t g_device_model_clock;

/* 命令状态 */
typedef enum {
    COMMAND_STATE_OK = 0,
    COMMAND_STATE_RUNNING,
    COMMAND_STATE_FAILD,
    COMMAND_STATE_TIMEOUT
} command_state_t;

/* 寄存器读写结果 */
typedef enum {
    DEVICE_MODEL_OK = 0,
    DEVICE_MODEL_BAD_ADDRESS,
    DEVICE_MODEL_BAD_VALUE
} device_model_result_t;

/* 初始化寄存器默认值；在 Gateway_Init() 中调用 */
void Device_Model_Init(void);

/* 读单个保持寄存器 */
device_model_result_t Device_Model_ReadSingal(uint8_t num, uint16_t *value);

/* 读多个保持寄存器。 */
device_model_result_t Device_Model_Read(can_frame_t *frame, uint16_t *value);

/* CAN 0x101 状态帧更新。 */
void Device_Model_UpdateStatus(const device_state_t *status);

/* CAN 0x201 心跳帧更新：记录收到心跳的时间。 */
void Device_Model_UpdateHeartbeat(const device_heart_t *heart, uint32_t now_ms);

/* CAN 0x401 ACK更新：根据ACK结果更新命令结果 */
void Device_Model_UpdateACK(const device_ack_t *ack, uint32_t now_ms);

/* 直接设置命令状态 */
void Device_Model_Cmd_State(uint8_t state);

/*
 * 周期性检查心跳超时：超过 1500ms 未收到 0x201 则标记离线
 * 返回 true 表示本次调用发生了在线 -> 离线切换
 * 周期性检查心跳超时：超过 1000ms 未收到 ack 则标记ack超时
 * 返回 true 表示本次调用发生了ack超时
 */
bool Device_Model_HeartbeatTick(uint32_t now_ms);
bool Device_Model_ACKTick(uint32_t now_ms);

/* 直接设置在线状态：0 在线，1 离线。 */
void Device_Model_SetOnline(uint16_t online);

#endif
