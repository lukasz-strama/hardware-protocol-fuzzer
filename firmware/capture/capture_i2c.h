/**
 * @file capture_i2c.h
 * @brief Interfejs sniffera magistrali I2C opartego na RP2040 PIO.
 */
#ifndef CAPTURE_I2C_H
#define CAPTURE_I2C_H

#include "hardware/pio.h"
#include <stdint.h>

/**
 * @brief Inicjalizuje sniffer I2C i przygotowuje PIO.
 *
 * @param sda Pin SDA.
 * @param scl Pin SCL (musi być sda+1).
 */
void capture_i2c_init(uint8_t sda, uint8_t scl);
/**
 * @brief Zatrzymuje sniffer I2C i zwalnia wszystkie zasoby PIO.
 */
void capture_i2c_stop(void);
/**
 * @brief Uruchamia oba state machine sniffera I2C.
 */
void capture_i2c_start(void);
/**
 * @brief Przetwarza dane odebrane przez PIO.
 *
 * Funkcja powinna być wywoływana cyklicznie.
 */
void capture_i2c_poll(void);
/**
 * @brief Obsługa przerwań PIO dla warunków START/STOP.
 */
void on_i2c_pio_irq(void);

#endif // CAPTURE_I2C_H
