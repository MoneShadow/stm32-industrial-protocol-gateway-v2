/*
 * 通用环形缓冲区（定长元素，SPSC）
 *
 * 使用模型：
 *   - 只允许一个生产者调用 RingBuffer_Push，一个消费者调用 RingBuffer_Pop；
 *   - 典型组合：中断 Push + 主循环 Pop（CAN 接收）
 *               主循环 Push + 中断 Pop（CAN 发送）；
 *   - 若同一端会被多个上下文并发调用，需要调用者自行加临界区。
 *
 * 容量约束：
 *   - capacity 必须是 2 的幂，范围 1 ~ 32768；
 *   - 存储区由调用者提供，大小至少 capacity * elem_size 字节；
 *   - 元素按字节拷贝，不保存指针，可存放任意定长类型（can_frame_t、编码帧等）。
 *
 * 满/空处理：
 *   - 队列满时 RingBuffer_Push 返回 false，丢弃新数据，不覆盖旧数据；
 *   - 队列空时 RingBuffer_Pop 返回 false，输出参数保持不变。
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *storage;       /* 调用者提供的存储区 */
    uint16_t capacity;      /* 元素容量，必须是 2 的幂 */
    uint16_t elem_size;     /* 单个元素的字节数 */
    volatile uint16_t head; /* 生产者写索引，只有生产者修改 */
    volatile uint16_t tail; /* 消费者读索引，只有消费者修改 */
} ring_buffer_t;

/* 初始化成功返回 true；参数非法时返回 false，之后 Push/Pop 均失败 */
bool RingBuffer_Init(ring_buffer_t *rb, void *storage, uint16_t capacity, uint16_t elem_size);

/* 生产者调用：入队一个元素。队列满返回 false */
bool RingBuffer_Push(ring_buffer_t *rb, const void *elem);

/* 消费者调用：出队一个元素。队列空返回 false */
bool RingBuffer_Pop(ring_buffer_t *rb, void *elem);

/* 当前元素个数。并发访问时只用于观察，不要用它做入队/出队的判据 */
uint16_t RingBuffer_Count(const ring_buffer_t *rb);

/* 队列是否为空 */
bool RingBuffer_IsEmpty(const ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
