/* temp_task.c */
#include "temp_task.h"
#include "ds18b20.h"

int16_t g_temp_x100 = 0;
uint8_t g_temp_valid = 0;       /* 上电还没读到，先标无效 */

#define PERIOD_MS   1000        /* 每次采样周期 */
#define CONV_MS     800         /* 转换等待，比手册 750ms 多留余量 */

typedef enum { ST_IDLE, ST_CONV } st_t;

void temp_task(void)
{
    static st_t st = ST_IDLE;
    static uint32_t t0 = 0;
    uint32_t now = HAL_GetTick();   /* [HAL] 毫秒计数，由 SysTick 中断累加 */
    int32_t t;

    switch (st) {
    case ST_IDLE:
        if (now - t0 >= PERIOD_MS) {
            t0 = now;
            if (ds18b20_start() == DS_OK) st = ST_CONV;
            else g_temp_valid = 0;          /* 没传感器：标无效，下个周期再试 */
        }
        break;
    case ST_CONV:
        if (now - t0 >= CONV_MS) {
            if (ds18b20_fetch(&t) == DS_OK) {
                g_temp_x100 = (int16_t)t;   /* 范围 -5500~12500，int16 装得下 */
                g_temp_valid = 1;
            } else {
                g_temp_valid = 0;           /* CRC 错或掉线：不给假数据 */
            }
            st = ST_IDLE;
        }
        break;
    }
}