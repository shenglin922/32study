#include "ds18b20.h"
#include "delay.h"

#define DS_PORT   GPIOB
#define DS_PIN    GPIO_PIN_12
#define DS_LOW()  (DS_PORT->BRR  = DS_PIN)
#define DS_HIGH() (DS_PORT->BSRR = DS_PIN)
#define DS_READ() ((DS_PORT->IDR & DS_PIN) ? 1 : 0)

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

static void ds_write_bit(uint8_t b)
{
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    DS_LOW();
    if (b) { delay_us(2);  DS_HIGH(); delay_us(60); }
    else   { delay_us(60); DS_HIGH(); delay_us(2);  }
    __set_PRIMASK(p);
}

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

static void ds_write_byte(uint8_t v)
{
    for (int i = 0; i < 8; i++) { ds_write_bit(v & 1); v >>= 1; }   /* 低位先发 */
}

static uint8_t ds_read_byte(void)
{
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) v |= (ds_read_bit() << i);
    return v;
}

int32_t ds18b20_raw_to_x100(int16_t raw)
{
    return (int32_t)raw * 625 / 100;     /* 每 LSB = 0.0625℃ */
}

int ds18b20_read_x100(int32_t *temp_x100)
{
    int r = ds18b20_start();
    if (r != DS_OK) return r;
    HAL_Delay(750);
    return ds18b20_fetch(temp_x100);
}

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

int ds18b20_start(void)
{
    if (!ds_reset()) return DS_ERR_NO_DEVICE;
    ds_write_byte(0xCC);                    /* Skip ROM */
    ds_write_byte(0x44);                    /* Convert T */
    return DS_OK;
}

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