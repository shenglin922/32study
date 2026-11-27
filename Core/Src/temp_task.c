/* temp_task.c */
#include "temp_task.h"
#include "ds18b20.h"
#include "cmsis_os.h"

int16_t  g_temp_x100     = 0;        /* 最近一次有效温度（0.01℃），由 TempTask 更新 */
uint8_t  g_temp_valid    = 0;        /* 1 = 温度数据可信；0 = 传感器故障或尚未完成首次采样 */
int16_t  g_alarm_th_x100 = 3000;    /* 高温报警阈值（默认 30.00℃），可通过 Modbus 0x06 修改 */
uint16_t g_dev_status    = 2;        /* 设备状态：0=正常, 1=高温报警, 2=传感器故障 */

typedef enum { ST_IDLE, ST_CONV } st_t;

/**
 * @brief  根据当前温度与报警阈值更新 g_dev_status 及板载 LED（PC13）状态
 * @note   必须在 g_reg_mutex 锁定期间调用（此函数本身不加锁）
 *         PC13 低电平点亮（漏极开路）：超温亮灯报警，其余状态灭灯
 */
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
/**
 * @brief  RTOS 温度采集任务单次步骤（供 StartTempTask 在 for(;;) 中调用）
 * @note   执行流程：
 *           1. ds18b20_start() 发出转换命令
 *           2. osDelay(800ms) 交出 CPU，让 ModbusTask 在等待期间正常响应请求
 *           3. ds18b20_fetch() 读取结果
 *           4. 加互斥锁更新共享寄存器（g_temp_x100、g_temp_valid、g_dev_status 及 LED）
 *           5. osDelay(200ms) 凑足 1 秒采样周期
 */
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