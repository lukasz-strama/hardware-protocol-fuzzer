#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "capture_uart.pio.h"
#include "usb_transport.h"
#include "session.h"
#include "capture_common.h"
#include "tusb.h"

int main(void) {
    session_init();
    usb_transport_init();   // tusb_init + protocol parser

    while (1) {
        // odbieranie komend
        usb_transport_task();

        
        if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING ||
            g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
            capture_task();
        }
    }
}