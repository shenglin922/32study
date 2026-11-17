#ifndef __RINGBUF_H
#define __RINGBUF_H
#include <stdint.h>

#define RB_SIZE 128

typedef struct {
    uint8_t buf[RB_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuf;

void rb_init(RingBuf *rb);
int  rb_write(RingBuf *rb, uint8_t data);
int  rb_read(RingBuf *rb, uint8_t *data);
int  rb_is_empty(RingBuf *rb);
int  rb_is_full(RingBuf *rb);
#endif