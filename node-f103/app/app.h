#ifndef APP_H
#define APP_H

/* 应用层初始化；在 Can_Port_Init() 之后调用一次 */
void App_Init(void);

/* 应用层主任务；在主循环里反复调用 */
void App_Task(void);

#endif /* APP_H */
