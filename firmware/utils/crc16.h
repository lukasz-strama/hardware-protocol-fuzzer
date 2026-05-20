#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

uint16_t crc16_ccitt_false(const uint8_t *data, size_t length, uint16_t initial_crc);

#endif