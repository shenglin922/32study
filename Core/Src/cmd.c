#include "cmd.h"
#include "string.h"
#include "ctype.h"
#include "stm32f1xx_hal.h"
#include "main.h"

extern UART_HandleTypeDef huart1;

uint8_t line_buf[64];
uint8_t line_len = 0;
uint8_t line_overflow=0;

void uart_send_str(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)s, strlen(s), 100);
}

typedef struct {
    const char *name;
    void (*handler)(void);
} Command;

static void cmd_led_on(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    uart_send_str("OK\r\n");
}
static void cmd_led_off(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    uart_send_str("OK\r\n");
}
static void cmd_led_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    uart_send_str("OK\r\n");
}

static void cmd_help(void);

static const Command cmd_table[] = {
    {"LED ON",     cmd_led_on},
    {"LED OFF",    cmd_led_off},
    {"LED TOGGLE", cmd_led_toggle},
    {"HELP",       cmd_help},
};
#define CMD_NUM (sizeof(cmd_table) / sizeof(cmd_table[0]))


static void cmd_help(void)
{
    uint8_t i;
    for (i = 0; i < CMD_NUM; i++)
    {
        uart_send_str(cmd_table[i].name);
        uart_send_str("\r\n");
    }
}

/* 去掉首尾空格，中间多个空格压成一个，转大写 */
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
