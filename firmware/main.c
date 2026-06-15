#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "usb_transport.h"
#include "session.h"
#include "capture_common.h"
#include "fuzz_engine.h"
#include "fuzz/fuzz_worker.h"
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

    /* Launch Core 1 fuzz worker */
    multicore_launch_core1(fuzz_worker_task);

#if I2C_SELFTEST_MODE
    init_master_i2c_generator();
    uint32_t last_tx_time = time_us_32();
    uint8_t test_data[] = {0xAA, 0xBB, 0xCC};
#endif

    while (1) {
        usb_transport_task();
        protocol_task();
        
        /* Check for timeout violations */
        session_check_timeouts();

        /* Core 0: Capture polling only (fuzz runs on Core 1) */
        if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING ||
            g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
            capture_task();
        }

#if I2C_SELFTEST_MODE
        if (time_us_32() - last_tx_time > 1500000) {
            last_tx_time = time_us_32();
            i2c_write_blocking(i2c1, 0x42, test_data, sizeof(test_data), false);
        }
#endif

        /* Brief sleep to prevent busy-loop */
        sleep_us(10);
    }

    return 0;
}