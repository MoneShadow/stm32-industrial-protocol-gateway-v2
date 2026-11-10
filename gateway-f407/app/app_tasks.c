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