/**
 * @file fuzz_uart_tx.h
 * @brief GPIO bit-bang UART transmitter for fuzz stimulus injection.
 *
 * Uses raw GPIO toggling (not PIO) so that we can deliberately inject
 * physical-layer errors: wrong parity, bad stop bits, and timing
 * distortion between bytes.
 */
#ifndef FUZZ_UART_TX_H
#define FUZZ_UART_TX_H

#include <stdint.h>
#include <stddef.h>

/** Insert a deliberately wrong parity bit (turns 8N1 into 8E1-with-error). */
#define FUZZ_TX_FLAG_CORRUPT_PARITY  (1u << 0)
/** Drive the stop bit low to inject a framing error. */
#define FUZZ_TX_FLAG_BAD_STOP_BIT    (1u << 1)
/** Random extra inter-byte delays to stress receiver gap handling. */
#define FUZZ_TX_FLAG_TIMING_DISTORT  (1u << 2)

/**
 * @brief Initialise the UART TX pin for bit-bang transmission.
 *
 * Configures the pin as GPIO output, idles high (UART idle = mark).
 *
 * @param tx_pin  GPIO number for TX output.
 * @param baud    Target baud rate (used to compute bit period).
 */
void fuzz_uart_tx_init(uint8_t tx_pin, uint32_t baud);

/**
 * @brief Release the TX pin back to high-Z input.
 */
void fuzz_uart_tx_deinit(void);

/**
 * @brief Transmit a buffer of bytes with optional error injection.
 *
 * @param data   Pointer to bytes to transmit.
 * @param len    Number of bytes.
 * @param flags  Bitmask of FUZZ_TX_FLAG_* options.
 */
void fuzz_uart_tx_send(const uint8_t *data, size_t len, uint8_t flags);

#endif /* FUZZ_UART_TX_H */
