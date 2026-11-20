#ifndef __DS18B20_H
#define __DS18B20_H
#include "main.h"

/* 返回 0 成功，-1 没检测到传感器；温度单位 0.01℃ */
int ds18b20_read_x100(int32_t *temp_x100);
int32_t ds18b20_raw_to_x100(int16_t raw);   /* 纯计算，便于假数据测试 */
#endif