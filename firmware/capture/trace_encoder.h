#ifndef TRACE_ENCODER_H
#define TRACE_ENCODER_H

#include <stdint.h>

#define TRACE_SOURCE_I2C 0
#define TRACE_SOURCE_UART 1

#define TRACE_EVENT_BYTE 0
#define TRACE_EVENT_START 1
#define TRACE_EVENT_STOP 2
#define TRACE_EVENT_ACK 3
#define TRACE_EVENT_NACK 4
#define TRACE_EVENT_BREAK 5
#define TRACE_EVENT_OVERFLOW 6

void trace_emit(uint32_t timestamp_us,
                uint8_t source_bus,
                uint8_t event_type,
                const uint8_t *data,
                uint16_t len);

#endif
