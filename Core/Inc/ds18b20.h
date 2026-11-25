#ifndef __DS18B20_H
#define __DS18B20_H

#define DS_OK 0
#define DS_ERR_NO_DEVICE -1   //未检测到传感器
#define DS_ERR_CRC -2   //数据监测失败
#include "main.h"

/* 返回 0 成功，-1 没检测到传感器；温度单位 0.01℃ */
int ds18b20_read_x100(int32_t *temp_x100);
int32_t ds18b20_raw_to_x100(int16_t raw);   /* 纯计算，便于假数据测试 */
uint8_t ds18b20_crc8(const uint8_t *data,uint8_t len);   /* 纯计算，便于假数据测试 */
int ds18b20_start(void);
int ds18b20_fetch(int32_t *temp_x100);
#endif