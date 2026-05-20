#ifndef FUZZ_UART_TX_H
#define FUZZ_UART_TX_H

#include <stdint.h>
#include <stddef.h>

#define FUZZ_TX_FLAG_CORRUPT_PARITY  (1u << 0)
#define FUZZ_TX_FLAG_BAD_STOP_BIT    (1u << 1)
#define FUZZ_TX_FLAG_TIMING_DISTORT  (1u << 2)

void fuzz_uart_tx_init(uint8_t tx_pin, uint32_t baud);
void fuzz_uart_tx_deinit(void);
void fuzz_uart_tx_send(const uint8_t *data, size_t len, uint8_t flags);

#endif
