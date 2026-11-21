#include "modbus_crc.h"

uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;                    /* Modbus 规定初值 */
    while (len--) {
        crc ^= *data++;                       /* 新字节混进低 8 位 */
        for (int i = 0; i < 8; i++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else         crc >>= 1;
        }
    }
    return crc;
}