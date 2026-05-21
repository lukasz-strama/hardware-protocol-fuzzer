#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RINGBUFFER_SIZE 2048

typedef struct {
    uint8_t buffer[RINGBUFFER_SIZE];
    volatile size_t head;
    volatile size_t tail;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb);
bool ring_buffer_is_empty(const ring_buffer_t *rb);
bool ring_buffer_is_full(const ring_buffer_t *rb);
bool ring_buffer_push(ring_buffer_t *rb, uint8_t data);
bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data);
size_t ring_buffer_available(const ring_buffer_t *rb);

#endif // RINGBUFFER_H