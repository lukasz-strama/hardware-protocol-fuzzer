/**
 * @file timestamp.c
 * @brief Prosty moduł dostarczający znacznik czasu w mikrosekundach.
 * Moduł wykorzystuje 32-bitowy licznik sprzętowy, który jest aktualizowany co mikrosekundę.
 * Licznik przepełnia się co ~71 minut (2^32 µs).
 */
#include "timestamp.h"
#include "hardware/timer.h"

/**
 * @brief Inicjalizuje moduł timestamp.
 */
void timestamp_init(void) {
    
}

/**
 * @brief Zwraca aktualny czas w mikrosekundach.
 *
 * Wartość pochodzi z rejestru, który:
 * - jest 32‑bitowy,
 * - inkrementuje się co 1 µs,
 * - przepełnia się automatycznie.
 *
 * @return Aktualny timestamp w mikrosekundach.
 */
uint32_t timestamp_get_us(void) {
    
    return timer_hw->timerawl;
}