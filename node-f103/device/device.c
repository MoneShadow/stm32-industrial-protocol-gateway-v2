#include "device.h"

#include "can_app.h"
#include "can_port.h"

#include "stm32f1xx_hal.h"

/* main.c 中由 ADC DMA 持续更新：[0] 温度原始值，[1] 光照原始值 */
extern volatile uint16_t g_adc_raw[2];

static device_model_t s_model;
static uint32_t s_update_period_ms = DEVICE_DEFAULT_UPDATE_PERIOD_MS;
static uint32_t s_last_update_ms;

void Device_Init(void) {
    s_model = (device_model_t){0};
    s_update_period_ms = DEVICE_DEFAULT_UPDATE_PERIOD_MS;
    s_last_update_ms = HAL_GetTick();

    /* 上电先采样一次，保证第一个 3 秒周期内有有效值 */
    Device_UpdateModel();
}

void Device_UpdateModel(void) {
    /*
     * TODO: 结合热敏电阻的分压特性，把 ADC 原始值换算成协议要求的
     *       温度 ×10（0.1℃）和光照工程量。当前先透传 ADC 原始值。
     */
    s_model.tempture = g_adc_raw[0];
    s_model.light = g_adc_raw[1];
}

void Device_Task(void) {
    uint32_t now = HAL_GetTick();

    /* 使用无符号差值，HAL_GetTick() 约 49 天回绕一次也不会出错 */
    if ((uint32_t)(now - s_last_update_ms) < s_update_period_ms) {
        return;
    }

    s_last_update_ms = now;
    Device_UpdateModel();
    (void)Device_SendStateFrame();
}

bool Device_SetUpdatePeriodMs(uint32_t period_ms) {
    if (period_ms == 0U) {
        return false;
    }

    s_update_period_ms = period_ms;
    /* 修改周期后从当前时刻重新计时，避免沿用旧周期的时间点 */
    s_last_update_ms = HAL_GetTick();
    return true;
}

uint32_t Device_GetUpdatePeriodMs(void) {
    return s_update_period_ms;
}

const device_model_t *Device_GetModel(void) {
    return &s_model;
}

void Device_SetFanRpm(uint16_t rpm) {
    s_model.fan_rpm = rpm;
}

void Device_SetLedState(uint16_t state) {
    s_model.led_state = (state != 0U) ? 1U : 0U;
}

int8_t Device_EncodeStateFrame(can_frame_t *frame) {
    return Can_App_State_Code(&s_model, frame);
}

can_port_result_t Device_SendStateFrame(void) {
    can_frame_t frame = {0};

    if (Device_EncodeStateFrame(&frame) != 0) {
        return CAN_PORT_ERROR;
    }

    return Can_Port_send(&frame);
}
