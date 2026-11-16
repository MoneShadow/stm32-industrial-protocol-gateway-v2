/* Device Model：保存 Modbus 寄存器对应的设备状态。 */
#include "device_model.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "command_manager.h"

#include <stddef.h>
#include <string.h>

#define DEVICE_MODEL_REG_COUNT      7U
#define DEVICE_MODEL_VERSION        1U

#define DEVICE_MODEL_ACK_TIMEOUT_MS 1000U
#define DEVICE_MODEL_HEARTBEAT_TIMEOUT_MS 1500U

/* 设备模型 */
typedef struct {
    uint16_t temperature;
    uint16_t light;
    uint16_t fan_rpm;
    uint16_t led_state;
    uint16_t cmd_result;        // 0成功 1在途 2失败 3超时
    uint16_t online;
    uint16_t protocol_version;
    bool heartbeat_seen;
    uint32_t last_heartbeat_ms;
    uint32_t new_command_ms;
} device_model_t;

static device_model_t s_model;
SemaphoreHandle_t g_device_model_clock;

void Device_Model_Init(void) {
    g_device_model_clock = xSemaphoreCreateMutex();
    while (g_device_model_clock == NULL) {
        /* 初始化失败 强行运行下去 也会导致后面的程序出现问题 所以没有必要继续运行 应该停下来 检查问题 所以此处死循环 */
    }
    memset(&s_model, 0, sizeof(s_model));
    s_model.online = 1U;              /* 1 = 离线，等待 CAN 心跳 */
    s_model.protocol_version = DEVICE_MODEL_VERSION;
}

/* 
    0   temperature
    1   light
    2   fan_rpm
    3   led_state
    4   online
*/
device_model_result_t Device_Model_ReadSingal(uint8_t num, uint16_t *value) {
    if (num >= DEVICE_MODEL_REG_COUNT) {
        return DEVICE_MODEL_BAD_ADDRESS;
    }
    switch (num) {
        case 0:
            *value = s_model.temperature;
            break;
        case 1:
            *value = s_model.light;
            break;
        case 2:
            *value = s_model.fan_rpm;
            break;
        case 3:
            *value = s_model.led_state;
            break;
        case 4:
            *value = s_model.cmd_result;
            break;
        case 5:
            *value = s_model.online;
            break;
        case 6:
            *value = s_model.protocol_version;
            break;
        default:
            return DEVICE_MODEL_BAD_ADDRESS;
            break;
    }
    return DEVICE_MODEL_OK;
}

device_model_result_t Device_Model_Read(can_frame_t *frame, uint16_t *value) {
    xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
    uint8_t j = 0;
    /* 检查需要检查7次 但是实际上需要读取的寄存器数量是一定的 所以 j 的大小是被严格限制的 */
    for (uint8_t i = 0; i < DEVICE_MODEL_REG_COUNT; i++) {
        if (frame->data[i]) {
            if ((Device_Model_ReadSingal(i, &value[j++]) != DEVICE_MODEL_OK) || j > frame->dlc) {
                xSemaphoreGive(g_device_model_clock);
                return DEVICE_MODEL_BAD_ADDRESS;
            }
        }
    }
    xSemaphoreGive(g_device_model_clock);
    return DEVICE_MODEL_OK;
}

void Device_Model_UpdateStatus(const device_state_t *status) {
    if (status == NULL) {
        return;
    }
    xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
    s_model.temperature = status->temperature;
    s_model.light = status->light;
    s_model.fan_rpm = status->fan_rpm;
    s_model.led_state = status->led_state;
    xSemaphoreGive(g_device_model_clock);
}

void Device_Model_UpdateHeartbeat(const device_heart_t *heart, uint32_t now_ms) {
    if (heart == NULL) {
        return;
    }
    xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
    s_model.online = 0U;
    s_model.heartbeat_seen = true;
    s_model.last_heartbeat_ms = now_ms;
    xSemaphoreGive(g_device_model_clock);
}

/* 0成功 1在途 2失败 3超时 */
void Device_Model_Cmd_State(uint8_t state) {
    if (state > 3) {
        return;
    }
    xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
    if (state == 1) {
        /* 产生新的命令需要记录一下命令发出的开始时间戳 */
        s_model.new_command_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    }
    s_model.cmd_result = state;
    xSemaphoreGive(g_device_model_clock);
}

void Device_Model_UpdateACK(const device_ack_t *ack, uint32_t now_ms) {
    command_t last_command;
    if (ack == NULL) {
        return;
    }
    /* 只 Peek 不取出 真正取出交由下面完成 */
    if (xQueuePeek(g_command_queue, &last_command, pdMS_TO_TICKS(100)) != pdPASS) {
        /* Peek 不出来数据 一般来讲是不可能的 所以此处留下一个测试点 */
        return;
    }
    if (ack->commandcode == last_command.command && ack->seq == last_command.seq) {
        /* 来到这里 说明ACK与上一条提交的命令对应 真正从队列中取出命令 给下一个命令让位 */
        xQueueReceive(g_command_queue, &last_command, pdMS_TO_TICKS(100));
        /* 更新对应的命令结果 */
        Device_Model_Cmd_State((!ack->result ? COMMAND_STATE_OK : COMMAND_STATE_FAILD));
    }
    else {
        /* ACK不匹配 等待下一条ACK 或者 等待ACK超时逻辑更新命令状态 这里不应该更新命令状态 */
    }
}

bool Device_Model_HeartbeatTick(uint32_t now_ms) {
    if (!s_model.heartbeat_seen) {
        return false;
    }

    if ((uint32_t)(now_ms - s_model.last_heartbeat_ms) >= DEVICE_MODEL_HEARTBEAT_TIMEOUT_MS) {
        xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
        s_model.online = 1U;
        s_model.heartbeat_seen = false;
        xSemaphoreGive(g_device_model_clock);
        return true;
    }

    return false;
}

/* 应该只在命令在途时生效(就是产生了新的命令) */
bool Device_Model_ACKTick(uint32_t now_ms) {
    if (s_model.cmd_result != 1) {
        /* 上一条命令发送成功的情况下 不进入超时逻辑 因为下一条命令还没参数 继续进行超时判断无意义 如果产生了新命令会更新命令状态和上一次的ack时间 */
        return false;
    }

    /* 新的命令已经产生 判断是否超时 */
    if ((uint32_t)(now_ms - s_model.new_command_ms) >= DEVICE_MODEL_ACK_TIMEOUT_MS) {
        xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
        /* 超时 清除已提交的命令 */
        command_t timeout_command = {0};
        xQueueReceive(g_command_queue, &timeout_command, 0);
        s_model.cmd_result = 3U;
        xSemaphoreGive(g_device_model_clock);
        return true;
    }

    return false;
}

void Device_Model_SetOnline(uint16_t online) {
    xSemaphoreTake(g_device_model_clock, portMAX_DELAY);
    s_model.online = online;
    xSemaphoreGive(g_device_model_clock);
}
