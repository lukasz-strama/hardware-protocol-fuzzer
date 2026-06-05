/**
 * @file timestamp.h
 * @brief Interfejs modułu timestamp dla RP2040.
 *
 * Zapewnia prosty dostęp do znacznika czasu w mikrosekundach.
 */
#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>

/**
 * @brief Inicjalizuje moduł timestamp.
 */
void timestamp_init(void);

/**
 * @brief Zwraca aktualny czas w mikrosekundach.
 *
 * @return 32‑bitowy licznik mikrosekund.
 */
uint32_t timestamp_get_us(void);

#endif // TIMESTAMP_H