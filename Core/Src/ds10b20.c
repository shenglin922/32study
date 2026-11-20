#include "ds18b20.h"
#include "delay.h"

#define DS_PORT   GPIOB
#define DS_PIN    GPIO_PIN_12
#define DS_LOW()  (DS_PORT->BRR  = DS_PIN)
#define DS_HIGH() (DS_PORT->BSRR = DS_PIN)
#define DS_READ() ((DS_PORT->IDR & DS_PIN) ? 1 : 0)

static uint8_t ds_reset(void)
{
    uint8_t present;
    DS_LOW();
    delay_us(480);
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    DS_HIGH();
    delay_us(70);
    present = (DS_READ() == 0);
    __set_PRIMASK(p);
    delay_us(410);
    return present;
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
    if (!ds_reset()) return -1;
    ds_write_byte(0xCC);                 /* Skip ROM */
    ds_write_byte(0x44);                 /* Convert T */
    HAL_Delay(750);                      /* 12 位精度最长 750ms */

    if (!ds_reset()) return -1;
    ds_write_byte(0xCC);
    ds_write_byte(0xBE);                 /* Read Scratchpad */
    uint8_t lsb = ds_read_byte();
    uint8_t msb = ds_read_byte();
    ds_reset();                          /* 复位以终止后面 7 字节的读取 */

    *temp_x100 = ds18b20_raw_to_x100((int16_t)((msb << 8) | lsb));
    return 0;
}