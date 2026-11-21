#include "modbus.h"
#include "modbus_crc.h"
#include "ringbuf.h"
#include <stdio.h>
#include <string.h>

extern RingBuf rx_rb;                
extern UART_HandleTypeDef huart1;

#define MB_SLAVE_ADDR   0x01
#define MB_GAP_MS       5               /* 帧间静默超时，先放宽，后面再收紧 */

static uint8_t  frame[MB_MAX_FRAME];
static uint16_t flen = 0;
static uint32_t last_rx_tick = 0;

static void mb_handle_frame(const uint8_t *f, uint16_t len)
{
    char buf[48];

    if (len < 4) return;                                   /* 太短，丢弃 */
    if (modbus_crc16(f, len) != 0) {                       /* 连 CRC 一起算，应为 0 */
        sprintf(buf, "MB: crc error\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
        return;
    }
    if (f[0] != MB_SLAVE_ADDR) return;                     /* 不是发给我的，静默忽略 */

    if (f[1] == 0x03 && len == 8) {
        uint16_t addr = (f[2] << 8) | f[3];                /* 大端 */
        uint16_t num  = (f[4] << 8) | f[5];
        sprintf(buf, "MB: read addr=%u num=%u\r\n", addr, num);
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
    } else {
        sprintf(buf, "MB: unsupported fc=%02X\r\n", f[1]);
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
    }
}

void modbus_poll(void)
{
    uint8_t c;
    while (rb_read(&rx_rb, &c) == 0) {                   
        if (flen < MB_MAX_FRAME) frame[flen++] = c;
        last_rx_tick = HAL_GetTick();
    }
    if (flen > 0 && (HAL_GetTick() - last_rx_tick) > MB_GAP_MS) {
        mb_handle_frame(frame, flen);
        flen = 0;
    }
}