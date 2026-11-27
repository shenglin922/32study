/* temp_task.c */
#include "temp_task.h"
#include "ds18b20.h"
#include "cmsis_os.h"

int16_t g_temp_x100 = 0;
uint8_t g_temp_valid = 0;       /* 上电还没读到，先标无效 */
int16_t g_alarm_th_x100 = 3000;     /* 默认高温报警阈值：30.00℃ */
uint16_t g_dev_status   = 2;        /* 0=正常, 1=高温报警, 2=传感器故障 */


// #define PERIOD_MS   1000        /* 每次采样周期 */
// #define CONV_MS     800         /* 转换等待，比手册 750ms 多留余量 */

typedef enum { ST_IDLE, ST_CONV } st_t;

static void update_alarm_state(void)
{
    if (!g_temp_valid) {
        g_dev_status = 2;
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    /* 故障灭灯 */
    } else if (g_temp_x100 >= g_alarm_th_x100) {
        g_dev_status = 1;
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  /* 超温亮灯报警 */
    } else {
        g_dev_status = 0;
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    /* 正常灭灯 */
    }
}
// 裸机函数
// void temp_task(void)
// {
//     static st_t st = ST_IDLE;
//     static uint32_t t0 = 0;
//     uint32_t now = HAL_GetTick();   /* [HAL] 毫秒计数，由 SysTick 中断累加 */
//     int32_t t;

//     switch (st) {
//     case ST_IDLE:
//         if (now - t0 >= PERIOD_MS) {
//             t0 = now;
//             if (ds18b20_start() == DS_OK) st = ST_CONV;
//             else g_temp_valid = 0;          /* 没传感器：标无效，下个周期再试 */
//         }
//         break;
//     case ST_CONV:
//         if (now - t0 >= CONV_MS) {
//             if (ds18b20_fetch(&t) == DS_OK) {
//                 g_temp_x100 = (int16_t)t;   /* 范围 -5500~12500，int16 装得下 */
//                 g_temp_valid = 1;
//             } else {
//                 g_temp_valid = 0;           /* CRC 错或掉线：不给假数据 */
//             }
//             update_alarm_state();
//             st = ST_IDLE;
//         }
//         break;
//     }
// }
void temp_task_rtos_step(void)
{
    int32_t t = 0;
    uint8_t ok = 0;

    /* 1. 启动温度转换（耗时的 800ms 期间绝不能占着互斥锁！） */
    if (ds18b20_start() == DS_OK) {
        osDelay(800);   /* 交出 CPU，让 ModbusTask 正常响应上位机 */
        if (ds18b20_fetch(&t) == DS_OK) {
            ok = 1;
        }
    }

    /* 2. 拿到数据后，瞬间加锁更新共享寄存器与 LED 状态（微秒级完成） */
    if (xSemaphoreTake(g_reg_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (ok) {
            g_temp_x100  = (int16_t)t;
            g_temp_valid = 1;
        } else {
            g_temp_valid = 0;
        }
        update_alarm_state();
        xSemaphoreGive(g_reg_mutex);
    }

    /* 3. 凑足 1 秒采样周期 */
    osDelay(200);
}