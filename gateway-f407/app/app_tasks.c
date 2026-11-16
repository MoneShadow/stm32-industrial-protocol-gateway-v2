#include "FreeRTOS.h"
#include "task.h"
#include "lwip.h"
#include "ethernetif.h"
#include "app_tasks.h"

#include "can_port.h"
#include "net_port.h"
#include "gateway.h"
#include "command_manager.h"

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
    if (Net_Port_Init(502) != NET_PORT_OK) {
        Error_Handler();
    }
    Gateway_Init();
    xTaskCreate(Gateway_ModbusRxTask, "modbus", 768, NULL, 2, NULL);
    xTaskCreate(Gateway_CanRxTask, "canrx", 512, NULL, 2, NULL);
    xTaskCreate(GatewayTask, "gw", 768, NULL, 2, NULL);
    xTaskCreate(CommandTask, "cmd", 768, NULL, 2, NULL);
    xTaskCreate(NetServerTask, "net", 768, NULL, 2, NULL);
    xTaskCreate(EthInputTask, "ethin", 512, NULL, 2, NULL);
    vTaskDelete(NULL);
}
