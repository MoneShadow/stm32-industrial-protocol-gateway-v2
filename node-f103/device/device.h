#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "can_port.h"

/* 默认刷新/上报周期：3 秒 */
#define DEVICE_DEFAULT_UPDATE_PERIOD_MS 3000U

/*
 * 本模块接口默认在主循环上下文调用；模型和 CAN 发送都不做中断保护，
 * 不要在中断里调用 Device_Task()/Device_SendStateFrame()。
 */

/*
 * 设备模型：保存下位机当前状态。
 * 字段使用 CPU 字节序；0x101 编码时由 Can_App_State_Code 转成协议要求的小端。
 */
typedef struct {
    uint16_t tempture;  /* 温度，当前保存 ADC 原始值 */
    uint16_t light;     /* 光照，当前保存 ADC 原始值 */
    uint16_t fan_rpm;   /* 风扇转速，由命令处理更新 */
    uint16_t led_state; /* LED 状态：0 灭，1 亮 */
} device_model_t;

/* 初始化模型和周期任务；在 Can_Port_Init() 之后调用 */
void Device_Init(void);

/* 主循环调用：每 period 毫秒刷新模型并发送一次 0x101 */
void Device_Task(void);

/* 修改刷新/上报周期，单位 ms；0 无效，返回 false */
bool Device_SetUpdatePeriodMs(uint32_t period_ms);

/* 读取当前周期，单位 ms */
uint32_t Device_GetUpdatePeriodMs(void);

/* 立即从 ADC 采样刷新模型，不发送 */
void Device_UpdateModel(void);

/* 读取当前模型（只读） */
const device_model_t *Device_GetModel(void);

/* 命令执行后更新模型字段；只在主循环上下文调用 */
void Device_SetFanRpm(uint16_t rpm);
void Device_SetLedState(uint16_t state);

/* 读取模型并编码成 0x101 帧，不发送；成功返回 0 */
int8_t Device_EncodeStateFrame(can_frame_t *frame);

/* 读取模型 -> 编码 0x101 -> Can_Port_send；返回发送结果 */
can_port_result_t Device_SendStateFrame(void);

#endif /* DEVICE_H */
