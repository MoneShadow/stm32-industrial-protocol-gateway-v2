#include "app.h"

#include "can_app.h"
#include "can_frame.h"
#include "can_port.h"
#include "device.h"
#include "ring_buffer.h"

#include "main.h"
#include "tim.h"

#define APP_NODE_ID 1U
#define APP_HEARTBEAT_PERIOD_MS 500U
#define APP_FAN_RPM_MAX 3000U
#define APP_FAN_PWM_MAX 500U /* TIM2 周期 1000，50% 对应 500 */

static uint32_t s_last_heartbeat_ms;
static uint8_t s_heartbeat_seq;

static void App_ApplyFan(uint16_t rpm) {
    uint32_t compare = ((uint32_t)rpm * APP_FAN_PWM_MAX) / APP_FAN_RPM_MAX;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, compare);
}

static void App_ApplyLed(uint16_t state) {
    /* LED 低电平点亮：亮 -> RESET，灭 -> SET */
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
                      (state != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void App_SendAck(const can_protocol_msg_t *msg, uint8_t result) {
    can_frame_t frame;

    if (Can_App_Ack_Frame_Code(msg, result, &frame) == 0) {
        (void)Can_Port_send(&frame);
    }
}

static void App_HandleCommand(can_frame_t *frame) {
    can_protocol_msg_t msg = {0};

    if (Can_App_Deco(frame, &msg) != 0) {
        return;
    }

    switch (msg.commandcode) {
    case 0x01U: /* 风扇转速 */
        if (msg.fan_rpm > APP_FAN_RPM_MAX) {
            App_SendAck(&msg, 2U); /* 2 = 非法 */
            return;
        }
        Device_SetFanRpm(msg.fan_rpm);
        App_ApplyFan(msg.fan_rpm);
        break;

    case 0x02U: /* LED */
        if (msg.led_state > 1U) {
            App_SendAck(&msg, 2U);
            return;
        }
        Device_SetLedState(msg.led_state);
        App_ApplyLed(msg.led_state);
        break;

    default:
        return;
    }

    App_SendAck(&msg, 0U);

    /* 命令改变模型后立即上报一次，网关不必等下一个 3 秒周期 */
    (void)Device_SendStateFrame();
}

static void App_CanTask(void) {
    can_frame_t frame;

    while (RingBuffer_Pop(&can_port_Rx_Ring, &frame)) {
        App_HandleCommand(&frame);
    }
}

static void App_HeartbeatTask(void) {
    uint32_t now = HAL_GetTick();
    can_frame_t frame;

    if ((uint32_t)(now - s_last_heartbeat_ms) < APP_HEARTBEAT_PERIOD_MS) {
        return;
    }
    s_last_heartbeat_ms = now;

    if (Can_App_Heart_Code(APP_NODE_ID, s_heartbeat_seq, &frame) == 0) {
        (void)Can_Port_send(&frame);
    }
    ++s_heartbeat_seq;
}

void App_Init(void) {
    s_heartbeat_seq = 0U;
    s_last_heartbeat_ms = HAL_GetTick();

    Device_Init();

    /* 上电先按模型初始值输出，避免保留 CubeMX 里的 50% 占空比 */
    App_ApplyFan(Device_GetModel()->fan_rpm);
    App_ApplyLed(Device_GetModel()->led_state);
}

void App_Task(void) {
    App_CanTask();     /* 处理 0x301 命令并回 0x401 */
    App_HeartbeatTask(); /* 每 500ms 发 0x201 */
    Device_Task();     /* 每 3s 刷新模型并发 0x101 */
}
