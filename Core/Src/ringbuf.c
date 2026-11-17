#include "ringbuf.h"

void rb_init(RingBuf *rb) { rb->head = 0; rb->tail = 0; }

int rb_is_empty(RingBuf *rb) { return rb->head == rb->tail; }

int rb_is_full(RingBuf *rb) { return ((rb->head + 1) % RB_SIZE) == rb->tail; }

int rb_write(RingBuf *rb, uint8_t data)
{
    if (rb_is_full(rb)) return -1;
    rb->buf[rb->head] = data;
    rb->head = (rb->head + 1) % RB_SIZE;
    return 0;
}

int rb_read(RingBuf *rb, uint8_t *data)
{
    if (rb_is_empty(rb)) return -1;
    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RB_SIZE;
    return 0;
}
