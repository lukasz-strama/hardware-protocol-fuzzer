#ifndef CAPTURE_UART_H
#define CAPTURE_UART_H

#include <stdint.h>
#include <stddef.h>
#include "trace_encoder.h"



void capture_uart_init(uint8_t rx_pin, uint32_t baud);
void capture_uart_start(void);
void capture_uart_stop(void);
void capture_uart_poll(void);

#endif
