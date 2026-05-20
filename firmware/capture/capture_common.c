#include "capture_common.h"
#include "capture_i2c.h"
#include "capture_uart.h"

void capture_task(void) {
    if (g_session.active_bus == TARGET_BUS_I2C) {
       // capture_i2c_poll();
    } else if (g_session.active_bus == TARGET_BUS_UART) {
        capture_uart_poll();
    }
}

void capture_prepare(const sniffer_session_t *session) {
    if (session->active_bus == TARGET_BUS_I2C) {
        //capture_i2c_init(session->i2c_sda_pin, session->i2c_scl_pin);
        //capture_i2c_start();
    } else if (session->active_bus == TARGET_BUS_UART) {
        capture_uart_init(session->uart_rx_pin, session->uart_baudrate);
        capture_uart_start();
    }
}

void capture_stop(void) {
    //capture_i2c_stop();
    capture_uart_stop();
}