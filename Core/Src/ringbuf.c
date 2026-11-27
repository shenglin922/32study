#include "ringbuf.h"

/* 初始化环形缓冲区，将读写指针均置零 */
void rb_init(RingBuf *rb) { rb->head = 0; rb->tail = 0; }

/* 判断环形缓冲区是否为空（head == tail 时为空），返回 1 表示空 */
int rb_is_empty(RingBuf *rb) { return rb->head == rb->tail; }

/* 判断环形缓冲区是否已满（留一个空位区分满/空状态），返回 1 表示满 */
int rb_is_full(RingBuf *rb) { return ((rb->head + 1) % RB_SIZE) == rb->tail; }

/**
 * @brief  向环形缓冲区写入一个字节
 * @param  rb   缓冲区指针
 * @param  data 待写入字节
 * @return 0 = 成功；-1 = 缓冲区已满，丢弃本次数据
 */
int rb_write(RingBuf *rb, uint8_t data)
{
    if (rb_is_full(rb)) return -1;
    rb->buf[rb->head] = data;
    rb->head = (rb->head + 1) % RB_SIZE;
    return 0;
}

/**
 * @brief  从环形缓冲区读取一个字节
 * @param  rb   缓冲区指针
 * @param  data 读出字节的存放地址
 * @return 0 = 成功；-1 = 缓冲区为空，无数据可读
 */
int rb_read(RingBuf *rb, uint8_t *data)
{
    if (rb_is_empty(rb)) return -1;
    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RB_SIZE;
    return 0;
}
