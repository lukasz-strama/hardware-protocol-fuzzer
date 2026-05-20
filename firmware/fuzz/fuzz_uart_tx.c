#include "fuzz_uart_tx.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static uint8_t  s_tx_pin;
static uint32_t s_bit_us;
static uint32_t s_prng;

static uint32_t prng_next(void) {
    s_prng ^= s_prng << 13;
    s_prng ^= s_prng >> 17;
    s_prng ^= s_prng << 5;
    return s_prng;
}

void fuzz_uart_tx_init(uint8_t tx_pin, uint32_t baud) {
    s_tx_pin = tx_pin;
    s_bit_us = (baud > 0) ? (1000000u / baud) : 9u;
    if (s_bit_us == 0) s_bit_us = 1;
    s_prng = 0xABCD1234u ^ (uint32_t)tx_pin ^ baud;

    gpio_init(tx_pin);
    gpio_set_dir(tx_pin, GPIO_OUT);
    gpio_put(tx_pin, 1);  /* UART idle = high */
}

void fuzz_uart_tx_deinit(void) {
    gpio_set_dir(s_tx_pin, GPIO_IN);
    gpio_disable_pulls(s_tx_pin);
}

static void send_byte(uint8_t byte, uint8_t flags) {
    /* Start bit */
    gpio_put(s_tx_pin, 0);
    sleep_us(s_bit_us);

    /* 8 data bits, LSB first */
    uint8_t parity = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t bit = (byte >> i) & 1u;
        parity ^= bit;
        gpio_put(s_tx_pin, bit);
        sleep_us(s_bit_us);
    }

    /*
     * Parity bit injection: normally 8N1 has no parity bit.
     * CORRUPT_PARITY inserts a deliberately wrong parity bit, turning the
     * frame into 8E1-with-error, which causes a parity framing error at the
     * receiver — exactly what we want to test.
     */
    if (flags & FUZZ_TX_FLAG_CORRUPT_PARITY) {
        gpio_put(s_tx_pin, parity ^ 1u);  /* inverted even-parity = always wrong */
        sleep_us(s_bit_us);
    }

    /* Stop bit: should be 1; BAD_STOP_BIT sends 0 to inject a framing error */
    if (flags & FUZZ_TX_FLAG_BAD_STOP_BIT) {
        gpio_put(s_tx_pin, 0);
    } else {
        gpio_put(s_tx_pin, 1);
    }
    sleep_us(s_bit_us);

    /* Return to idle */
    gpio_put(s_tx_pin, 1);
}

void fuzz_uart_tx_send(const uint8_t *data, size_t len, uint8_t flags) {
    for (size_t i = 0; i < len; i++) {
        send_byte(data[i], flags);

        /*
         * Timing distortion: random extra delay between bytes, 0..3x bit period.
         * This exercises receiver timeout and inter-byte gap handling.
         */
        if (flags & FUZZ_TX_FLAG_TIMING_DISTORT) {
            uint32_t extra_us = prng_next() % (3u * s_bit_us + 1u);
            if (extra_us > 0) {
                sleep_us(extra_us);
            }
        }
    }
}
