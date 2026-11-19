#ifndef __CMD_H
#define __CMD_H
#include <stdint.h>

void uart_send_str(const char *s);
void cmd_feed_byte(uint8_t c);//收到一个字节就调用一次
#endif