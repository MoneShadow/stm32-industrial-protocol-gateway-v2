#include "FreeRTOS.h"
#include "task.h"
#include "lwip.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "ethernetif.h"
#include "app_tasks.h"

#include "can_port.h"
#include "can_app.h"

void TestTask(void *arg) {
    (void)arg;
    for (;;) {
        /* 用你的 LED 或 UART 证明任务在跑 */
        HAL_GPIO_TogglePin(LED_Green_GPIO_Port, LED_Green_Pin);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void EthInputTask(void *arg) {
    (void)arg;
    for (;;) {
        ethernetif_input(&gnetif);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void InitTask(void *arg) {
    (void)arg;
    MX_LWIP_Init();
    if (Can_Port_Init() != CAN_PORT_OK) {
        Error_Handler();
    }
    xTaskCreate(EthInputTask, "ethin", 512, NULL, 2, NULL);
    vTaskDelete(NULL);
}

void Can_Send_Command_Test(void *arg) {
    (void)arg;
    can_frame_t frame = {0};
    while (1) {
        Can_App_Fan_Code(3000, 1, &frame);
        Can_Port_send(&frame);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

can_frame_t frame1 = {0};
void Can_Recv_Command_Test(void *arg) {
    (void)arg;
    while (1) {
        Can_Port_recv(&frame1, UINT32_MAX);
    }
}