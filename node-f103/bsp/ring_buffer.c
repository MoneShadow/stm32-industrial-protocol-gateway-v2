#include "ring_buffer.h"

#include <string.h>

/*
 * 编译器屏障：保证元素数据的拷贝不会被编译器重排到索引更新之后。
 * Cortex-M3 单核、无 D-Cache，只需编译器屏障，不需要硬件内存屏障。
 */
#if defined(__GNUC__)
#define RING_BUFFER_BARRIER() __asm__ volatile("" ::: "memory")
#else
#define RING_BUFFER_BARRIER() ((void)0)
#endif

static bool RingBuffer_IsPowerOfTwo(uint16_t value) {
    return (value != 0U) && ((value & (uint16_t)(value - 1U)) == 0U);
}

static bool RingBuffer_IsValid(const ring_buffer_t *rb) {
    return (rb != NULL) && (rb->storage != NULL) && (rb->capacity != 0U) &&
           (rb->elem_size != 0U);
}

bool RingBuffer_Init(ring_buffer_t *rb, void *storage, uint16_t capacity, uint16_t elem_size) {
    /* capacity 上限 32768：16 位自由递增索引需要能无歧义表示 capacity 个元素 */
    if ((rb == NULL) || (storage == NULL) || (elem_size == 0U) ||
        (!RingBuffer_IsPowerOfTwo(capacity)) || (capacity > 32768U)) {
        if (rb != NULL) {
            rb->storage = NULL;
            rb->capacity = 0U;
            rb->elem_size = 0U;
            rb->head = 0U;
            rb->tail = 0U;
        }
        return false;
    }

    rb->storage = (uint8_t *)storage;
    rb->capacity = capacity;
    rb->elem_size = elem_size;
    rb->head = 0U;
    rb->tail = 0U;
    return true;
}

bool RingBuffer_Push(ring_buffer_t *rb, const void *elem) {
    uint16_t head;
    uint16_t tail;
    uint16_t index;

    if ((!RingBuffer_IsValid(rb)) || (elem == NULL)) {
        return false;
    }

    head = rb->head;
    tail = rb->tail;
    /* 自由递增索引之差即元素个数，回绕后仍然成立 */
    if ((uint16_t)(head - tail) >= rb->capacity) {
        return false;
    }

    index = (uint16_t)(head & (uint16_t)(rb->capacity - 1U));
    memcpy(&rb->storage[(uint32_t)index * rb->elem_size], elem, rb->elem_size);

    RING_BUFFER_BARRIER(); /* 数据写完之后再发布 head */
    rb->head = (uint16_t)(head + 1U);
    return true;
}

bool RingBuffer_Pop(ring_buffer_t *rb, void *elem) {
    uint16_t head;
    uint16_t tail;
    uint16_t index;

    if ((!RingBuffer_IsValid(rb)) || (elem == NULL)) {
        return false;
    }

    tail = rb->tail;
    head = rb->head;
    if (head == tail) {
        return false;
    }

    index = (uint16_t)(tail & (uint16_t)(rb->capacity - 1U));
    memcpy(elem, &rb->storage[(uint32_t)index * rb->elem_size], rb->elem_size);

    RING_BUFFER_BARRIER(); /* 数据读完再释放 tail */
    rb->tail = (uint16_t)(tail + 1U);
    return true;
}

uint16_t RingBuffer_Count(const ring_buffer_t *rb) {
    if (!RingBuffer_IsValid(rb)) {
        return 0U;
    }
    return (uint16_t)(rb->head - rb->tail);
}

bool RingBuffer_IsEmpty(const ring_buffer_t *rb) {
    return RingBuffer_Count(rb) == 0U;
}
