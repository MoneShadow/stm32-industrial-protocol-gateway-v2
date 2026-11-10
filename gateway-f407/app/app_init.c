#include "FreeRTOS.h"
#include "task.h"
#include "app_tasks.h"

void App_Init() {
    xTaskCreate(TestTask, "test", 256, NULL, 2, NULL);
    xTaskCreate(InitTask,  "init", 1024, NULL, 2, NULL);
    vTaskStartScheduler();
}