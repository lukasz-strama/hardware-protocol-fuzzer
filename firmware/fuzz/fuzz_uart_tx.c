/**
 * @file fuzz_uart_tx.c
 * @brief GPIO bit-bang UART transmitter with deliberate error injection.
 *
 * Bit-bangs 8N1 frames on a GPIO pin.  The caller can request:
 *  - corrupt parity (inserts a wrong extra parity bit → framing error),
 *  - bad stop bit (stop bit driven low → framing error),
 *  - timing distortion (random inter-byte gaps).
 *
 * This is intentionally *not* PIO-based: we need full control over each
 * bit period and the ability to insert malformed bits.
 */
#include "fuzz_uart_tx.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/* ── Internal state ─────────────────────────────────────────────── */

static uint8_t  s_tx_pin;
static uint32_t s_bit_us;     /* microseconds per bit */
static uint32_t s_prng;       /* xorshift32 state      */

/* ── Simple xorshift32 PRNG ─────────────────────────────────────── */

static uint32_t prng_next(void)
{
    s_prng ^= s_prng << 13;
    s_prng ^= s_prng >> 17;
    s_prng ^= s_prng << 5;
    return s_prng;
}

/* ── Public API ─────────────────────────────────────────────────── */

void fuzz_uart_tx_init(uint8_t tx_pin, uint32_t baud)
{
    s_tx_pin = tx_pin;
    s_bit_us = (baud > 0) ? (1000000u / baud) : 9u;
    if (s_bit_us == 0) s_bit_us = 1;
    s_prng = 0xABCD1234u ^ (uint32_t)tx_pin ^ baud;

    gpio_init(tx_pin);
    gpio_set_dir(tx_pin, GPIO_OUT);
    gpio_put(tx_pin, 1);  /* UART idle = high (mark) */
}

void fuzz_uart_tx_deinit(void)
{
    gpio_put(s_tx_pin, 1);          /* return to idle before releasing */
    gpio_set_dir(s_tx_pin, GPIO_IN);
    gpio_disable_pulls(s_tx_pin);
}

/* ── Transmit one byte ──────────────────────────────────────────── */

static void send_byte(uint8_t byte, uint8_t flags)
{
    /* Start bit (always low) */
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
     * Optional parity-bit injection.
     * Normal 8N1 has no parity bit.  CORRUPT_PARITY inserts a
     * deliberately wrong parity bit, turning the frame into
     * 8E1-with-error — exactly what we want to stress-test.
     */
    if (flags & FUZZ_TX_FLAG_CORRUPT_PARITY) {
        gpio_put(s_tx_pin, parity ^ 1u);  /* inverted even parity = always wrong */
        sleep_us(s_bit_us);
    }

    /* Stop bit: should be 1; BAD_STOP_BIT sends 0 → framing error */
    gpio_put(s_tx_pin, (flags & FUZZ_TX_FLAG_BAD_STOP_BIT) ? 0 : 1);
    sleep_us(s_bit_us);

    /* Return to idle (high) */
    gpio_put(s_tx_pin, 1);
}

/* ── Transmit a buffer ──────────────────────────────────────────── */

void fuzz_uart_tx_send(const uint8_t *data, size_t len, uint8_t flags)
{
    for (size_t i = 0; i < len; i++) {
        send_byte(data[i], flags);

        /*
         * Timing distortion: random extra delay between bytes,
         * 0‥3× bit period.  Exercises receiver timeout and
         * inter-byte gap handling.
         */
        if (flags & FUZZ_TX_FLAG_TIMING_DISTORT) {
            uint32_t extra_us = prng_next() % (3u * s_bit_us + 1u);
            if (extra_us > 0) {
                sleep_us(extra_us);
            }
        }
    }
}
