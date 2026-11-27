#include "modbus_crc.h"

/**
 * @brief  计算 Modbus RTU CRC-16（多项式 0xA001，初值 0xFFFF，低字节先出）
 * @note   验证时将含 CRC 的完整帧传入，结果应为 0 表示数据正确
 * @param  data  待计算数据起始地址
 * @param  len   数据字节数（不含 CRC 本身）
 * @return 16 位 CRC 校验值（低字节先）
 */
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