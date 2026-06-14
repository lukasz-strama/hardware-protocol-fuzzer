/**
 * @file ringbuffer.c
 * @brief Implementacja prostego bufora pierścieniowego o stałym rozmiarze.
 *
 * Bufor działa w trybie FIFO i przechowuje bajty. W przypadku przepełnienia
 * operacja push zwraca false, a dane nie są nadpisywane.
 */
#include "ringbuffer.h"

/**
 * @brief Inicjalizuje bufor pierścieniowy.
 *
 * Ustawia wskaźniki head i tail na 0, co oznacza pusty bufor.
 *
 * @param rb Wskaźnik na strukturę bufora.
 */
void ring_buffer_init(ring_buffer_t* rb) {
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief Sprawdza, czy bufor jest pusty.
 *
 * @param rb Wskaźnik na bufor.
 * @return true jeśli bufor jest pusty.
 */
bool ring_buffer_is_empty(const ring_buffer_t* rb) {
    return rb->head == rb->tail;
}

/**
 * @brief Sprawdza, czy bufor jest pełny.
 *
 * Bufor jest pełny, gdy head znajduje się jeden krok przed tail.
 *
 * @param rb Wskaźnik na bufor.
 * @return true jeśli bufor jest pełny.
 */
bool ring_buffer_is_full(const ring_buffer_t* rb) {
    return ((rb->head + 1) % RINGBUFFER_SIZE) == rb->tail;
}

/**
 * @brief Dodaje bajt do bufora.
 *
 * Jeśli bufor jest pełny, funkcja zwraca false i nie nadpisuje danych.
 *
 * @param rb Wskaźnik na bufor.
 * @param data Bajt do zapisania.
 * @return true jeśli zapis się powiódł.
 */
bool ring_buffer_push(ring_buffer_t* rb, uint8_t data) {
    if (ring_buffer_is_full(rb)) {
        return false; // pełny
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RINGBUFFER_SIZE;
    return true;
}

/**
 * @brief Pobiera bajt z bufora.
 *
 * Jeśli bufor jest pusty, funkcja zwraca false.
 *
 * @param rb Wskaźnik na bufor.
 * @param data Wskaźnik na zmienną, do której zostanie zapisany bajt.
 * @return true jeśli odczyt się powiódł.
 */
bool ring_buffer_pop(ring_buffer_t* rb, uint8_t* data) {
    if (ring_buffer_is_empty(rb)) {
        return false; // pusty
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RINGBUFFER_SIZE;
    return true;
}

/**
 * @brief Zwraca liczbę bajtów aktualnie znajdujących się w buforze.
 *
 * @param rb Wskaźnik na bufor.
 * @return Liczba bajtów dostępnych do odczytu.
 */
size_t ring_buffer_available(const ring_buffer_t* rb) {
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return RINGBUFFER_SIZE - (rb->tail - rb->head);
    }
}