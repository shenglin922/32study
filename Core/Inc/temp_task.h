/* temp_task.h */
#ifndef __TEMP_TASK_H
#define __TEMP_TASK_H
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"

extern int16_t g_temp_x100;    /* 最近一次有效温度，0.01℃ */
extern uint8_t g_temp_valid;   /* 1 = 温度可信，0 = 传感器故障/尚未读到 */
extern int16_t g_alarm_th_x100;
extern uint16_t g_dev_status;

extern SemaphoreHandle_t g_reg_mutex;   /* 保护共享寄存器表的互斥锁 */
void temp_task_rtos_step(void);         /* RTOS 版单次采集与报警更新 */
void temp_task(void);
#endif