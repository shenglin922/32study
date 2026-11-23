#ifndef __MODBUS_H
#define __MODBUS_H
#include "main.h"

#define MB_MAX_FRAME 64
void modbus_poll(void);
int16_t g_temp_x100=2506;/*临时假数据*/
#endif

