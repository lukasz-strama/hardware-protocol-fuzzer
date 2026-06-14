/**
 * @file capture_uart.h
 * @brief Interfejs sniffera magistrali UART opartego na RP2040 PIO.
 */
#ifndef CAPTURE_UART_H
#define CAPTURE_UART_H

#include <stdint.h>
#include <stddef.h>
#include "trace_encoder.h"


/**
 * @brief Inicjalizuje sniffer UART i przygotowuje PIO.
 *
 * @param rx_pin Pin odbiorczy UART.
 * @param baud Prędkość transmisji UART.
 */
void capture_uart_init(uint8_t rx_pin, uint32_t baud);
/**
 * @brief Uruchamia state machine dekodujący UART.
 */
void capture_uart_start(void);
/**
 * @brief Zatrzymuje sniffer UART i zwalnia zasoby PIO.
 */
void capture_uart_stop(void);
/**
 * @brief Przetwarza dane odebrane przez PIO.
 *
 * @note Funkcja powinna być wywoływana cyklicznie w pętli głównej.
 */
void capture_uart_poll(void);

#endif
