#include "session.h"
#include "capture_common.h"
#include "pico/stdlib.h"
#include <string.h>

sniffer_session_t g_session;

void session_init(void) {
    memset(&g_session, 0, sizeof(g_session));
    g_session.current_state = HW_PROTOCOL_STATE_DETACHED;
    g_session.active_bus    = TARGET_BUS_NONE;
}

void session_handle_set_bus(const uint8_t *payload) {
    const hw_protocol_set_bus_t *p = (const hw_protocol_set_bus_t *)payload;

    if (p->bus_type == HW_PROTOCOL_BUS_UART) {
        g_session.active_bus    = TARGET_BUS_UART;
        g_session.uart_baudrate = p->speed_hz;
        g_session.uart_rx_pin   = p->pin_b;
        g_session.uart_tx_pin   = p->pin_a;
    } else if (p->bus_type == HW_PROTOCOL_BUS_I2C) {
        g_session.active_bus  = TARGET_BUS_I2C;
        g_session.i2c_sda_pin = p->pin_a;
        g_session.i2c_scl_pin = p->pin_b;
    }

    g_session.current_state = HW_PROTOCOL_STATE_CONFIGURED;
}

void session_handle_set_target(const uint8_t *p) {
    uint16_t vtarget_mv  = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    uint8_t  pullup_mode = p[3];
    uint8_t  pullup_mask = p[4];

    g_session.vtarget_mv  = vtarget_mv;
    g_session.pullup_mode = pullup_mode;
    g_session.pullup_mask = pullup_mask;

    if (g_session.current_state == HW_PROTOCOL_STATE_CONFIGURED ||
        g_session.current_state == HW_PROTOCOL_STATE_CONNECTED) {
        g_session.current_state = HW_PROTOCOL_STATE_CONFIGURED;
    }
}

void session_handle_arm(uint16_t session_id) {
    if (g_session.current_state != HW_PROTOCOL_STATE_CONFIGURED) return;
    g_session.session_id    = session_id;
    g_session.current_state = HW_PROTOCOL_STATE_ARMED;
    capture_prepare(&g_session);   
}

void session_handle_start_capture(void) {
    if (g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
        g_session.current_state = HW_PROTOCOL_STATE_RUNNING;
    }
}

uint32_t session_handle_stop(void) {
    if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING ||
        g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
        g_session.current_state = HW_PROTOCOL_STATE_STOPPING;
        capture_stop();
        g_session.current_state = HW_PROTOCOL_STATE_ARMED;
    }
    return 0;
}

void session_handle_disarm(void) {
    capture_stop();
    g_session.active_bus    = TARGET_BUS_NONE;
    g_session.current_state = HW_PROTOCOL_STATE_CONNECTED;
}