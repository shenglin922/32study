#include "cmd.h"
#include "string.h"
#include "ctype.h"
#include "stm32f1xx_hal.h"
#include "main.h"
#include "stdio.h"
#include "ds18b20.h"

extern UART_HandleTypeDef huart1;

uint8_t line_buf[64];
uint8_t line_len = 0;
uint8_t line_overflow=0;

/**
 * @brief  通过 UART1 发送以 null 结尾的字符串（阻塞，超时 100ms）
 * @param  s  待发送字符串
 */
void uart_send_str(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)s, strlen(s), 100);
}

typedef struct {
    const char *name;
    void (*handler)(void);
} Command;

/* 命令：点亮板载 LED（PC13 低电平有效） */
static void cmd_led_on(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    uart_send_str("OK\r\n");
}
/* 命令：熄灭板载 LED */
static void cmd_led_off(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    uart_send_str("OK\r\n");
}
/* 命令：翻转板载 LED 状态 */
static void cmd_led_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    uart_send_str("OK\r\n");
}

static void cmd_help(void);

/**
 * @brief  命令：阻塞式读取 DS18B20 温度并通过 UART1 输出（格式：TEMP:±XX.XX C）
 * @note   内部调用 ds18b20_read_x100（含 750ms HAL_Delay 阻塞）
 */
static void cmd_get_temp(void)
{
    int32_t t;
    char buf[32];
    int r = ds18b20_read_x100(&t);
    if(r==DS_ERR_NO_DEVICE){
        uart_send_str("ERROR: NO sensor not found\r\n");
        return;
    }
    else if(r==DS_ERR_CRC){
        uart_send_str("ERROR: CRC FAILED\r\n");
        return;
    }
    int32_t a =(t<0)? -t : t;
    sprintf(buf,"TEMP:%s%ld.%02ld C\r\n",
            (t<0)?"-":"",
            (long)(a/100),
            (long)(a%100));
    uart_send_str(buf);
}




static const Command cmd_table[] = {
    {"LED ON",     cmd_led_on},
    {"LED OFF",    cmd_led_off},
    {"LED TOGGLE", cmd_led_toggle},
    {"HELP",       cmd_help},
    {"GET TEMP",       cmd_get_temp},
};
#define CMD_NUM (sizeof(cmd_table) / sizeof(cmd_table[0]))


/* 命令：列出所有支持的命令名称 */
static void cmd_help(void){
    uint8_t i;
    for (i = 0; i < CMD_NUM; i++)
    {
        uart_send_str(cmd_table[i].name);
        uart_send_str("\r\n");
    }
}

/**
 * @brief  字符串规范化：去首尾空格、将连续空格压缩为单个空格、全部转大写
 * @param  s  原地修改的命令字符串
 */
static void normalize_cmd(char *s)
{
    char *dst = s;
    char *p;
    uint8_t in_space = 1;
    for (p = s; *p; p++)
    {
        if (*p == ' ' || *p == '\t')
        {
            if (!in_space) { *dst++ = ' '; in_space = 1; }
        }
        else
        {
            *dst++ = toupper((unsigned char)*p);
            in_space = 0;
        }
    }
    if (dst > s && *(dst - 1) == ' ') dst--;
    *dst = '\0';
}


/**
 * @brief  处理一条完整的命令字符串：先规范化再在命令表中查找并执行
 * @param  cmd  待处理命令字符串（会被原地修改为大写）
 */
static void process_command(char *cmd)
{
    uint8_t i;
    normalize_cmd(cmd);
    if (cmd[0] == '\0') return;
    for (i = 0; i < CMD_NUM; i++)
    {
        if (strcmp(cmd, cmd_table[i].name) == 0)
        {
            cmd_table[i].handler();
            return;
        }
    }
    uart_send_str("ERROR\r\n");
}

/**
 * @brief  字节流解析器：每次喂入一个字节，积累至换行符时触发命令处理
 * @note   行缓冲区 64 字节；溢出时报错并丢弃本行；忽略 '\r'
 *         由主循环或 CliTask 从 uart1_rb 中逐字节调用
 * @param  c  接收到的字节
 */
void cmd_feed_byte(uint8_t c){
    if(c=='\n'){
        if(line_overflow){
            uart_send_str("ERROE: TOO LONG\r\n");
            line_overflow=0;
        }
        else{
            line_buf[line_len]='\0';
            if(line_len>0) process_command((char*)line_buf);
        }
        line_len=0;
    }
    else if(c!='\r'){
        if(line_len<sizeof(line_buf)-1) line_buf[line_len++]=c;
        else line_overflow=1;
    }
}
