#include "ds18b20.h"
#include "delay.h"

#define DS_PORT   GPIOB
#define DS_PIN    GPIO_PIN_12
#define DS_LOW()  (DS_PORT->BRR  = DS_PIN)
#define DS_HIGH() (DS_PORT->BSRR = DS_PIN)
#define DS_READ() ((DS_PORT->IDR & DS_PIN) ? 1 : 0)

/**
 * @brief  DS18B20 复位脉冲：发出 ≥480µs 低脉冲，然后检测存在脉冲（15~60µs 低）
 * @note   关中断保证采样时刻精确；同时验证总线在存在脉冲后已恢复高，防止短路误判
 * @return 1 = 检测到传感器且总线释放正常；0 = 未检测到或总线故障
 */
static uint8_t ds_reset(void)
{
    uint8_t present, released;

    DS_HIGH();
    delay_us(5);
    if (DS_READ() == 0) return 0;     /* 空闲应为高，读到低 = 无上拉或短路到地 */

    DS_LOW();
    delay_us(480);                    /* [时序] 复位脉冲 ≥480µs（DS18B20 手册） */

    uint32_t p = __get_PRIMASK();     /* [内核] 保存中断状态 */
    __disable_irq();                  /* [内核] 关总中断，保证采样时刻精确 */
    DS_HIGH();
    delay_us(70);                     /* [时序] 存在脉冲窗口内采样 */
    present = (DS_READ() == 0);
    __set_PRIMASK(p);                 /* [内核] 恢复进来时的状态 */

    delay_us(410);                    /* 补足时隙 */
    released = DS_READ();             /* [时序] 存在脉冲最迟约 300µs 结束，此时总线必须已回高 */
    return present && released;
}

/**
 * @brief  向单总线写一个位（关中断保证时序精确）
 * @param  b  1 = 写 1 时隙（2µs 低 + 60µs 高）；0 = 写 0 时隙（60µs 低 + 2µs 高）
 */
static void ds_write_bit(uint8_t b)
{
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    DS_LOW();
    if (b) { delay_us(2);  DS_HIGH(); delay_us(60); }
    else   { delay_us(60); DS_HIGH(); delay_us(2);  }
    __set_PRIMASK(p);
}

/**
 * @brief  从单总线读一个位（主机拉低 2µs 释放，10µs 内采样，共 60µs 时隙）
 * @return 读到的位值：0 或 1
 */
static uint8_t ds_read_bit(void)
{
    uint8_t b;
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    DS_LOW();
    delay_us(2);
    DS_HIGH();
    delay_us(8);
    b = DS_READ();
    delay_us(50);
    __set_PRIMASK(p);
    return b;
}

/* 向单总线写一个字节（低位先发，符合 DS18B20 协议） */
static void ds_write_byte(uint8_t v)
{
    for (int i = 0; i < 8; i++) { ds_write_bit(v & 1); v >>= 1; }   /* 低位先发 */
}

/* 从单总线读一个字节（低位先收，8 次 ds_read_bit 组合） */
static uint8_t ds_read_byte(void)
{
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) v |= (ds_read_bit() << i);
    return v;
}

/**
 * @brief  将 DS18B20 Scratchpad 中的原始 16 位温度寄存器值换算为 0.01℃ 单位
 * @note   DS18B20 默认 12 位精度，1 LSB = 0.0625℃ = 625/10000℃
 *         raw * 0.0625 = raw * 625 / 10000 = raw * 625 / 100 / 100
 *         此处返回 raw * 625 / 100，即以 0.01℃ 为单位的整数值
 * @param  raw  有符号 16 位原始值（补码，来自 Scratchpad Byte 0 & 1）
 * @return 温度值（单位：0.01℃），例如 2506 表示 25.06℃
 */
int32_t ds18b20_raw_to_x100(int16_t raw)
{
    return (int32_t)raw * 625 / 100;     /* 每 LSB = 0.0625℃ */
}

/**
 * @brief  阻塞式读取温度（启动转换 → 等待 750ms → 读取结果）
 * @note   仅用于裸机/调试场景（RTOS 下请用 ds18b20_start + osDelay + ds18b20_fetch）
 * @param  temp_x100  温度结果存放地址（单位：0.01℃）
 * @return DS_OK / DS_ERR_NO_DEVICE / DS_ERR_CRC
 */
int ds18b20_read_x100(int32_t *temp_x100)
{
    int r = ds18b20_start();
    if (r != DS_OK) return r;
    HAL_Delay(750);
    return ds18b20_fetch(temp_x100);
}

/**
 * @brief  DS18B20 单线 CRC-8 校验（多项式 0x8C，Dallas/Maxim 格式）
 * @note   将 Scratchpad 全部 9 字节传入，结果为 0 表示数据无误
 * @param  data  数据起始地址
 * @param  len   数据长度
 * @return 8 位 CRC 值
 */
uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for(int i =0;i<8;i++){
            if(crc & 1)crc = (crc>>1)^0x8C;
            else crc >>= 1;
        }
    }
    return crc;
}

/**
 * @brief  启动 DS18B20 温度转换（发 Skip ROM + Convert T 命令）
 * @note   发完后须等待 ≥750ms 再调用 ds18b20_fetch 读取结果
 * @return DS_OK = 命令已发出；DS_ERR_NO_DEVICE = 未检测到传感器
 */
int ds18b20_start(void)
{
    if (!ds_reset()) return DS_ERR_NO_DEVICE;
    ds_write_byte(0xCC);                    /* Skip ROM */
    ds_write_byte(0x44);                    /* Convert T */
    return DS_OK;
}

/**
 * @brief  读取 DS18B20 Scratchpad 并解析温度（需在 ds18b20_start 后 ≥750ms 调用）
 * @note   验证 CRC-8 及 Byte5（保留字节应为 0xFF），任一不符则返回 DS_ERR_CRC
 *         解析完成后发送 Reset 终止 Scratchpad 读取
 * @param  temp_x100  温度结果存放地址（单位：0.01℃）
 * @return DS_OK / DS_ERR_NO_DEVICE / DS_ERR_CRC
 */
int ds18b20_fetch(int32_t *temp_x100)
{
    if (!ds_reset()) return DS_ERR_NO_DEVICE;
    ds_write_byte(0xCC);
    ds_write_byte(0xBE);                    /* Read Scratchpad */
    uint8_t sp[9];
    for (int i = 0; i < 9; i++) sp[i] = ds_read_byte();
    ds_reset();

    if (ds18b20_crc8(sp, 9) != 0) return DS_ERR_CRC;
    if (sp[5] != 0xFF)            return DS_ERR_CRC;
    *temp_x100 = ds18b20_raw_to_x100((int16_t)((sp[1] << 8) | sp[0]));
    return DS_OK;
}