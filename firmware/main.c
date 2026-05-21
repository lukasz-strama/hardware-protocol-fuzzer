#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "capture_uart.pio.h"
#include "usb_transport.h"
#include "session.h"
#include "capture_common.h"
#include "fuzz_engine.h"
#include "tusb.h"

int main(void) {
    session_init();
    usb_transport_init();

    while (1) {
        usb_transport_task();

        if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING) {
            if (g_session.fuzz_mode) {
                fuzz_engine_task();
                capture_task();  /* capture target responses during fuzz */
            } else {
                capture_task();
            }
        } else if (g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
            capture_task();
        }
    }
}