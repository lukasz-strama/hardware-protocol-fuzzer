#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "capture_uart.pio.h"
#include "tusb.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define SELFTEST_PIN      17
#define SELFTEST_BAUDRATE 115200

static void _cdc_print(const char *s) {
    tud_cdc_write(s, strlen(s));
    tud_cdc_write_flush();
}

void uart_selftest_run(void) {
    /* Inicjalizuj PIO */
    PIO pio    = pio0;
    uint offset = pio_add_program(pio, &uart_sniffer_program);
    uint sm    = pio_claim_unused_sm(pio, true);
    float div  = (float)clock_get_hz(clk_sys) / ((float)SELFTEST_BAUDRATE * 8.0f);

    pio_sm_config c = uart_sniffer_program_get_default_config(offset);
    sm_config_set_in_pins(&c, SELFTEST_PIN);
    sm_config_set_jmp_pin(&c, SELFTEST_PIN);
    sm_config_set_clkdiv(&c, div);
    sm_config_set_in_shift(&c, true, true, 8);
    pio_gpio_init(pio, SELFTEST_PIN);
    gpio_set_dir(SELFTEST_PIN, GPIO_IN);
    gpio_pull_up(SELFTEST_PIN);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    while (!tud_cdc_connected()) tud_task();
    _cdc_print("=== UART SELF-TEST ===\r\n");
    _cdc_print("GP17 @ 115200 baud. Podlacz urzadzenie docelowe.\r\n\r\n");

    char buf[64];
    uint32_t total_bytes = 0;
    uint32_t overflows   = 0;
    uint32_t last_report = to_ms_since_boot(get_absolute_time());

    while (1) {
        tud_task();

        /* Odbieraj bajty z PIO */
        while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
            uint8_t byte = (uint8_t)(pio_sm_get(pio, sm) & 0xFF);
            total_bytes++;
            snprintf(buf, sizeof(buf), "0x%02X '%c'\r\n",
                     byte, (byte >= 0x20 && byte < 0x7F) ? byte : '.');
            _cdc_print(buf);
        }

        if (pio->fdebug & (1u << (8 + sm))) {
            pio->fdebug = (1u << (8 + sm));
            overflows++;
            _cdc_print("!! OVERFLOW — za szybko !!\r\n");
        }

       
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_report >= 1000) {
            last_report = now;
            snprintf(buf, sizeof(buf),
                     "--- total=%lu overflows=%lu ---\r\n",
                     (unsigned long)total_bytes,
                     (unsigned long)overflows);
            _cdc_print(buf);
        }
    }
}