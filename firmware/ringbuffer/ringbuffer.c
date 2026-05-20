#include "ringbuffer.h"

void ring_buffer_init(ring_buffer_t* rb) {
    rb->head = 0;
    rb->tail = 0;
}

bool ring_buffer_is_empty(const ring_buffer_t* rb) {
    return rb->head == rb->tail;
}

bool ring_buffer_is_full(const ring_buffer_t* rb) {
    return ((rb->head + 1) % RINGBUFFER_SIZE) == rb->tail;
}

bool ring_buffer_push(ring_buffer_t* rb, uint8_t data) {
    if (ring_buffer_is_full(rb)) {
        return false; // pełny
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RINGBUFFER_SIZE;
    return true;
}

bool ring_buffer_pop(ring_buffer_t* rb, uint8_t* data) {
    if (ring_buffer_is_empty(rb)) {
        return false; // pusty
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RINGBUFFER_SIZE;
    return true;
}

size_t ring_buffer_available(const ring_buffer_t* rb) {
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return RINGBUFFER_SIZE - (rb->tail - rb->head);
    }
}