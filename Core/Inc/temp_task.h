/* temp_task.h */
#ifndef __TEMP_TASK_H
#define __TEMP_TASK_H
#include "main.h"
extern int16_t g_temp_x100;    /* 最近一次有效温度，0.01℃ */
extern uint8_t g_temp_valid;   /* 1 = 温度可信，0 = 传感器故障/尚未读到 */
void temp_task(void);
#endif