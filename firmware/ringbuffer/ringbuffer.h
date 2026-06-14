/**
 * @file ringbuffer.h
 * @brief Interfejs prostego bufora pierścieniowego FIFO.
 *
 * Bufor ma stały rozmiar RINGBUFFER_SIZE i przechowuje bajty.
 * Implementacja nie nadpisuje danych — przepełnienie powoduje błąd push().
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RINGBUFFER_SIZE 2048


/**
 * @brief Struktura bufora pierścieniowego.
 *
 * head - indeks zapisu  
 * tail - indeks odczytu  
 * buffer - tablica danych
 */
typedef struct {
    uint8_t buffer[RINGBUFFER_SIZE];
    volatile size_t head;
    volatile size_t tail;
} ring_buffer_t;

/**
 * @brief Inicjalizuje bufor pierścieniowy.
 */
void ring_buffer_init(ring_buffer_t *rb);
/**
 * @brief Sprawdza, czy bufor jest pusty.
 */
bool ring_buffer_is_empty(const ring_buffer_t *rb);
/**
 * @brief Sprawdza, czy bufor jest pełny.
 */
bool ring_buffer_is_full(const ring_buffer_t *rb);
/**
 * @brief Dodaje bajt do bufora.
 */
bool ring_buffer_push(ring_buffer_t *rb, uint8_t data);
/**
 * @brief Pobiera bajt z bufora.
 */
bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data);
/**
 * @brief Zwraca liczbę bajtów aktualnie znajdujących się w buforze.
 */
size_t ring_buffer_available(const ring_buffer_t *rb);

#endif // RINGBUFFER_H