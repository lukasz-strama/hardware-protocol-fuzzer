#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "usb_transport.h"
#include "session.h"
#include "capture_common.h"
#include "fuzz_engine.h"
#include "protocol/protocol.h" 

// 1 - test i2c master generator mode, 0 - normal operation without i2c generator
#define I2C_SELFTEST_MODE 1

#define I2C_MASTER_SDA_PIN 2
#define I2C_MASTER_SCL_PIN 3

void init_master_i2c_generator() {
    i2c_init(i2c1, 10 * 1000); 
    gpio_set_function(I2C_MASTER_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_MASTER_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MASTER_SDA_PIN);
    gpio_pull_up(I2C_MASTER_SCL_PIN);
}

int main(void) {
    stdio_init_all();
    session_init();
    usb_transport_init();

#if I2C_SELFTEST_MODE
    init_master_i2c_generator();
    uint32_t last_tx_time = time_us_32();
    uint8_t test_data[] = {0xAA, 0xBB, 0xCC};
#endif

    while (1) {
        usb_transport_task();
        protocol_task();

        if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING) {
            if (g_session.fuzz_mode) {
                fuzz_engine_task();
                capture_task();  
            } else {
                capture_task();
            }
        } else if (g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
            capture_task();
        }

#if I2C_SELFTEST_MODE
        if (time_us_32() - last_tx_time > 1500000) {
            last_tx_time = time_us_32();
            i2c_write_blocking(i2c1, 0x42, test_data, sizeof(test_data), false);
        }
#endif
    }

    return 0;
}