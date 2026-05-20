#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>

void timestamp_init(void);

uint32_t timestamp_get_us(void);

#endif // TIMESTAMP_H