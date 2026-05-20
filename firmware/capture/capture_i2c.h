#ifndef CAPTURE_I2C_H
#define CAPTURE_I2C_H

#include "hardware/pio.h"
#include <stdint.h>

void capture_i2c_init(uint8_t sda, uint8_t scl);
void capture_i2c_stop(void);
void capture_i2c_start(void);
void capture_i2c_poll(void);
void on_i2c_pio_irq(void);

#endif // CAPTURE_I2C_H
