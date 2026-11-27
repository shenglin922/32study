#include "modbus.h"
#include "modbus_crc.h"
#include "ringbuf.h"
#include <stdio.h>
#include <string.h>
#include "temp_task.h"

extern RingBuf uart2_rb;                
extern UART_HandleTypeDef huart2;
uint8_t rx2_byte;
uint8_t rx1_byte;

#define MB_SLAVE_ADDR   0x01
#define MB_GAP_MS       5               /* 帧间静默超时，先放宽，后面再收紧 */
#define REG_COUNT 3


static uint8_t  frame[MB_MAX_FRAME];
static uint16_t flen = 0;
static uint32_t last_rx_tick = 0;


/* 读寄存器表。0=温度, 1=设备状态, 2=报警阈值 */

/**
 * @brief  初始化 Modbus RTU 从站：初始化 UART2 接收环形缓冲区并启动中断接收
 * @note   在 osKernelStart() 之前、MX_USART2_UART_Init() 之后调用
 */
void modbus_init(void){
    rb_init(&uart2_rb);    
    HAL_UART_Receive_IT(&huart2,&rx2_byte,1);
}

/**
 * @brief  按地址读取保持寄存器当前值（无锁，调用前需持有 g_reg_mutex）
 * @param  addr  寄存器地址：0=温度(0.01℃), 1=设备状态, 2=报警阈值(0.01℃)
 * @return 寄存器值（uint16，补码原样传输）
 */
static uint16_t reg_read(uint16_t addr)
{
    switch (addr) {
    case 0: return (uint16_t)g_temp_x100;
    case 1: return g_dev_status;
    case 2: return (uint16_t)g_alarm_th_x100;
    default: return 0;
    }
}

/**
 * @brief  向 UART2 发送一帧并自动追加 CRC16（低字节在前）
 * @param  buf  帧缓冲区（buf[len] 和 buf[len+1] 将被 CRC 覆盖，调用方须预留空间）
 * @param  len  不含 CRC 的有效字节数
 */
static void mb_send(uint8_t *buf, uint16_t len)
{
    uint16_t crc = modbus_crc16(buf, len);
    buf[len]     = crc & 0xFF;          /* 低字节在前 */
    buf[len + 1] = crc >> 8;
    HAL_UART_Transmit(&huart2, buf, len + 2, 100);
}
/**
 * @brief  发送 Modbus 异常响应帧（功能码最高位置 1）
 * @param  fc    原始功能码
 * @param  code  异常码：01=非法功能码, 02=非法地址, 03=非法数据值, 04=设备故障
 */
static void mb_send_exception(uint8_t fc, uint8_t code)
{
    uint8_t r[5];
    r[0] = MB_SLAVE_ADDR;
    r[1] = fc | 0x80;                   /* 功能码最高位置 1 表示异常 */
    r[2] = code;
    mb_send(r, 3);
}


/**
 * @brief  解析并处理一帧完整的 Modbus RTU 请求
 * @note   支持功能码：
 *           - 0x03 读保持寄存器（Reg 0~2）
 *           - 0x06 写单个保持寄存器（仅 Reg 2 = 报警阈值可写，范围 -5500~12500）
 *         CRC 错误或非本机地址时静默丢弃；不支持的功能码返回异常码 01
 * @param  f    帧数据指针（含 CRC）
 * @param  len  帧总字节数（含 CRC）
 */
static void mb_handle_frame(const uint8_t *f, uint16_t len)
{
    if (len < 4) return;
    if (modbus_crc16(f, len) != 0) return;
    if (f[0] != MB_SLAVE_ADDR) return;

    uint8_t fc = f[1];

    /* ===== 功能码 0x03：读保持寄存器 ===== */
    if (fc == 0x03) {
        if (len != 8) { mb_send_exception(0x03, 0x03); return; }

        uint16_t addr = (f[2] << 8) | f[3];
        uint16_t num  = (f[4] << 8) | f[5];

        if (num == 0 || num > 125) { mb_send_exception(0x03, 0x03); return; }
        if (addr + num > REG_COUNT) { mb_send_exception(0x03, 0x02); return; }

        /* 只有读取包含 Reg 0（温度）且温度无效时，才报设备故障 04 */
        if (addr == 0 && !g_temp_valid) {
            mb_send_exception(0x03, 0x04);
            return;
        }

        uint8_t r[5 + 2 * 16];
        uint8_t sensor_err = 0;
        if (xSemaphoreTake(g_reg_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (addr == 0 && !g_temp_valid) {
                sensor_err = 1;
            } else {
                r[0] = MB_SLAVE_ADDR;
                r[1] = 0x03;
                r[2] = num * 2;
                for (uint16_t i = 0; i < num; i++) {
                    uint16_t v = reg_read(addr + i);
                    r[3 + 2 * i] = v >> 8;
                    r[4 + 2 * i] = v & 0xFF;
                }
            }
            xSemaphoreGive(g_reg_mutex);
        } else {
            mb_send_exception(0x03, 0x04);
            return;
        }

        if (sensor_err) {
            mb_send_exception(0x03, 0x04);
            return;
        }
        mb_send(r, 3 + 2 * num);
    }
    /* ===== 功能码 0x06：写单个保持寄存器 ===== */
    else if (fc == 0x06) {
        if (len != 8) { mb_send_exception(0x06, 0x03); return; }

        uint16_t addr = ((uint16_t)f[2] << 8) | f[3];
        int16_t  val  = (int16_t)(((uint16_t)f[4] << 8) | f[5]);

        if (addr >= REG_COUNT || addr != 2) {
            mb_send_exception(0x06, 0x02);
            return;
        }
        if (val < -5500 || val > 12500) {
            mb_send_exception(0x06, 0x03);
            return;
        }

        if (xSemaphoreTake(g_reg_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            g_alarm_th_x100 = val;
            xSemaphoreGive(g_reg_mutex);
        } else {
            mb_send_exception(0x06, 0x04);
            return;
        }

        uint8_t echo[8];
        memcpy(echo, f, 6);
        mb_send(echo, 6);
    }
}

/**
 * @brief  Modbus 轮询函数（供 StartModbusTask 在 for(;;) 中调用）
 * @note   从 uart2_rb 读取所有已到达字节并更新最后接收时刻；
 *         当缓冲区排空且距最后字节超过 MB_GAP_MS ms（帧间静默），
 *         视为一帧完整，调用 mb_handle_frame 处理后清零帧计数器
 */
void modbus_poll(void)
{
    uint8_t c;
    while (rb_read(&uart2_rb, &c) == 0) {                   
        if (flen < MB_MAX_FRAME) frame[flen++] = c;
        last_rx_tick = HAL_GetTick();
    }
    if (flen > 0 && (HAL_GetTick() - last_rx_tick) > MB_GAP_MS) {
        mb_handle_frame(frame, flen);
        flen = 0;
    }
}

