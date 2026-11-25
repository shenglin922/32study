#include "modbus.h"
#include "modbus_crc.h"
#include "ringbuf.h"
#include <stdio.h>
#include <string.h>
#include "temp_task.h"

extern RingBuf uart2_rb;                
extern UART_HandleTypeDef huart2;

#define MB_SLAVE_ADDR   0x01
#define MB_GAP_MS       5               /* 帧间静默超时，先放宽，后面再收紧 */
#define REG_COUNT 1


static uint8_t  frame[MB_MAX_FRAME];
static uint16_t flen = 0;
static uint32_t last_rx_tick = 0;

/* 读寄存器表。目前只有寄存器 0 = 温度 */
static uint16_t reg_read(uint16_t addr)
{
    if (addr == 0) return (uint16_t)g_temp_x100;   /* int16 转 uint16，补码原样传 */
    return 0;
}

/* 发送一帧：自动补 CRC（低字节在前） */
static void mb_send(uint8_t *buf, uint16_t len)
{
    uint16_t crc = modbus_crc16(buf, len);
    buf[len]     = crc & 0xFF;          /* 低字节在前 */
    buf[len + 1] = crc >> 8;
    HAL_UART_Transmit(&huart2, buf, len + 2, 100);
}
/*发送异常响应*/
static void mb_send_exception(uint8_t fc, uint8_t code)
{
    uint8_t r[5];
    r[0] = MB_SLAVE_ADDR;
    r[1] = fc | 0x80;                   /* 功能码最高位置 1 表示异常 */
    r[2] = code;
    mb_send(r, 3);
}


/*判断这帧内容处不处理，怎么处理*/
static void mb_handle_frame(const uint8_t *f, uint16_t len)
{
    if (len < 4) return;
    if (modbus_crc16(f, len) != 0) return;        /* CRC 错：静默丢弃 */
    if (f[0] != MB_SLAVE_ADDR) return;            /* 不是我：静默 */

    if (f[1] != 0x03 || len != 8) {
        mb_send_exception(f[1], 0x01);            /* 功能码不支持 */
        return;
    }

    uint16_t addr = (f[2] << 8) | f[3];
    uint16_t num  = (f[4] << 8) | f[5];

    if (num == 0 || num > 125) { mb_send_exception(0x03, 0x03); return; }
    if (addr + num > REG_COUNT) { mb_send_exception(0x03, 0x02); return; }
    if (!g_temp_valid) {
        mb_send_exception(0x03, 0x04);            /* 温度无效 / 传感器故障 */
        return;
    }

    uint8_t r[5 + 2 * 125];                       /* 最大帧，栈上约 255 字节 */
    r[0] = MB_SLAVE_ADDR;
    r[1] = 0x03;
    r[2] = num * 2;                               /* 字节数 */
    for (uint16_t i = 0; i < num; i++) {
        uint16_t v = reg_read(addr + i);
        r[3 + 2 * i] = v >> 8;                    /* 高字节在前 */
        r[4 + 2 * i] = v & 0xFF;
    }
    mb_send(r, 3 + 2 * num);
}

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

