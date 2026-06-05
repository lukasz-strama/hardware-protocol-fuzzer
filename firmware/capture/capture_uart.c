 /**
 * @file capture_uart.c
 * @brief Implementacja sniffera magistrali UART opartego na RP2040 PIO.
 *
 * Moduł wykorzystuje jeden state machine PIO do dekodowania ramek UART
 * w trybie LSB-first. Odebrane bajty są emitowane do systemu trace.
 */
#include "capture_uart.h"
#include "trace_encoder.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "capture_uart.pio.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

static PIO  uart_pio = pio0;     /**< PIO używane do dekodowania UART. */
static uint sm_uart;             /**< State machine odpowiedzialny za odbiór UART. */
static uint offset_uart;         /**< Offset programu PIO dekodującego UART. */

static uint32_t uart_baudrate = 115200; /**< Aktualna prędkość UART. */
static uint8_t  uart_rx_pin  = 0;       /**< Pin RX wykorzystywany przez sniffer. */

#ifndef PIO_FDEBUG_RXOVER_LSB
#define PIO_FDEBUG_RXOVER_LSB 8
#endif

/**
 * @brief Inicjalizuje sniffer UART i przygotowuje PIO do pracy.
 *
 * Funkcja:
 * - ładuje program PIO dekodujący UART,
 * - konfiguruje state machine,
 * - ustawia pin RX jako wejście z podciąganiem,
 * - oblicza dzielnik zegara tak, aby 1 cykl PIO = 1/8 bitu UART.
 *
 * @param rx_pin Pin odbiorczy UART.
 * @param baud Prędkość transmisji UART (np. 115200).
 *
 * @note Funkcja automatycznie uruchamia state machine.
 */
void capture_uart_init(uint8_t rx_pin, uint32_t baud) {
    uart_rx_pin  = rx_pin;
    uart_baudrate = baud;

    offset_uart = pio_add_program(uart_pio, &uart_sniffer_program);
    sm_uart     = pio_claim_unused_sm(uart_pio, true);

    /* 1 cykl PIO = 1/8 bitu */
    float div = (float)clock_get_hz(clk_sys) / ((float)baud * 8.0f);

    pio_sm_config c = uart_sniffer_program_get_default_config(offset_uart);
    sm_config_set_in_pins(&c, rx_pin);
    sm_config_set_jmp_pin(&c, rx_pin);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(uart_pio, rx_pin);
    gpio_set_dir(rx_pin, GPIO_IN);
    gpio_pull_up(rx_pin);  

    /* Autopush 8 bitów, LSB first */
    sm_config_set_in_shift(&c, true, true, 8);

    pio_sm_init(uart_pio, sm_uart, offset_uart, &c);
    pio_sm_set_enabled(uart_pio, sm_uart, true);
}

/**
 * @brief Przetwarza dane odebrane przez PIO.
 *
 * Funkcja:
 * - odczytuje FIFO state machine,
 * - dekoduje bajt (LSB-first - MSB-first),
 * - wypisuje bajt na UART debug (opcjonalnie),
 * - emituje zdarzenie TRACE_EVENT_BYTE,
 * - wykrywa overflow FIFO i raportuje TRACE_EVENT_OVERFLOW.
 *
 * @note Funkcja powinna być wywoływana cyklicznie w pętli głównej.
 */
void capture_uart_poll(void) {
    while (!pio_sm_is_rx_fifo_empty(uart_pio, sm_uart)) {
        uint32_t raw  = pio_sm_get(uart_pio, sm_uart);
        uint8_t  byte = (uint8_t)(raw >> 24);
        printf("UART BYTE: %02X\n", byte);

        trace_emit(time_us_32(), TRACE_SOURCE_UART, TRACE_EVENT_BYTE, &byte, 1);
    }

    uint32_t overflow_mask = (1u << (PIO_FDEBUG_RXOVER_LSB + sm_uart));
    if (uart_pio->fdebug & overflow_mask) {
        uart_pio->fdebug = overflow_mask;
        trace_emit(time_us_32(), TRACE_SOURCE_UART, TRACE_EVENT_OVERFLOW, NULL, 0);
    }
}

/**
 * @brief Włącza state machine odpowiedzialny za dekodowanie UART.
 *
 * @note State machine jest domyślnie włączony po inicjalizacji.
 */
void capture_uart_start(void) {
    pio_sm_set_enabled(uart_pio, sm_uart, true);
}

/**
 * @brief Zatrzymuje sniffer UART i zwalnia zasoby PIO.
 *
 * Funkcja:
 * - wyłącza state machine,
 * - usuwa program PIO,
 * - zwalnia state machine.
 *
 * @note Po wywołaniu tej funkcji sniffer wymaga ponownej inicjalizacji.
 */
void capture_uart_stop(void) {
    pio_sm_set_enabled(uart_pio, sm_uart, false);
    pio_remove_program(uart_pio, &uart_sniffer_program, offset_uart);
    pio_sm_unclaim(uart_pio, sm_uart);
}