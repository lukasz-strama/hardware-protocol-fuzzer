/**
 * @file crc16.h
 * @brief Interfejs funkcji obliczającej CRC16-CCITT-FALSE.
 *
 * Funkcja jest wykorzystywana w warstwie protokołu do obliczania sumy
 * kontrolnej nagłówków i payloadów.
 */
#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Oblicza CRC16-CCITT-FALSE.
 *
 * @param data        Bufor danych wejściowych.
 * @param length      Liczba bajtów.
 * @param initial_crc Wartość początkowa CRC (najczęściej 0xFFFF).
 *
 * @return Obliczona wartość CRC16.
 */
uint16_t crc16_ccitt_false(const uint8_t *data, size_t length, uint16_t initial_crc);

#endif