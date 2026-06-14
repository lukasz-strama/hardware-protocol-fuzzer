/**
 * @file crc16.c
 * @brief Implementacja algorytmu CRC16-CCITT-FALSE.
 *
 * Algorytm:
 * - polinom: 0x1021
 * - seed (initial value): 0xFFFF
 * - brak odbicia bitów (no reflection)
 * - brak XOR-out
 */
#include "crc16.h"


/**
 * @brief Oblicza CRC16-CCITT-FALSE dla podanego bufora.
 *
 * Algorytm:
 * - CRC ^= (data[i] << 8)
 * - 8 iteracji przesunięcia i XOR z polinomem 0x1021
 *
 * @param data      Wskaźnik na dane wejściowe.
 * @param length    Liczba bajtów danych.
 * @param initial_crc Wartość początkowa CRC (zwykle 0xFFFF).
 *
 * @return Obliczona wartość CRC16.
 *
 * @note Funkcja nie wykonuje odbicia bitów ani XOR-out.
 */
uint16_t crc16_ccitt_false(const uint8_t *data, size_t length, uint16_t initial_crc) {
    uint16_t crc = initial_crc;
    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}