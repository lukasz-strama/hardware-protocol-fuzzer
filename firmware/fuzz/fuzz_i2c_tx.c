/**
 * @file fuzz_i2c_tx.c
 * @brief GPIO bit-bang I2C master transmitter with error injection.
 *
 * Implements a software I2C master using open-drain GPIO toggling.
 * Supports deliberate protocol-level errors:
 *  - SKIP_ACK: ignore NACK and keep transmitting,
 *  - REPEATED_START: send Sr instead of STOP,
 *  - CLOCK_STRETCH: hold SCL low for extra half-periods,
 *  - NO_STOP: omit the STOP condition entirely.
 *
 * Open-drain emulation: to drive low → output 0, direction OUT.
 * To release high → direction IN, external pull-up pulls line high.
 */
#include "fuzz_i2c_tx.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/* ── Internal state ─────────────────────────────────────────────── */

static uint8_t  s_sda_pin;
static uint8_t  s_scl_pin;
static uint32_t s_half_period_us;  /* half of one SCL period */
static uint32_t s_prng;

/* ── Helpers ────────────────────────────────────────────────────── */

static uint32_t prng_next(void)
{
    s_prng ^= s_prng << 13;
    s_prng ^= s_prng >> 17;
    s_prng ^= s_prng << 5;
    return s_prng;
}

/** Drive SDA low (open-drain: set output 0, direction out). */
static inline void sda_low(void)
{
    gpio_put(s_sda_pin, 0);
    gpio_set_dir(s_sda_pin, GPIO_OUT);
}

/** Release SDA high (open-drain: set direction in, pull-up pulls high). */
static inline void sda_high(void)
{
    gpio_set_dir(s_sda_pin, GPIO_IN);
}

/** Read SDA level. */
static inline bool sda_read(void)
{
    return gpio_get(s_sda_pin);
}

/** Drive SCL low. */
static inline void scl_low(void)
{
    gpio_put(s_scl_pin, 0);
    gpio_set_dir(s_scl_pin, GPIO_OUT);
}

/** Release SCL high. */
static inline void scl_high(void)
{
    gpio_set_dir(s_scl_pin, GPIO_IN);
}

static inline void half_period(void)
{
    sleep_us(s_half_period_us);
}

/* ── I2C primitives ─────────────────────────────────────────────── */

/** Generate START condition: SDA high→low while SCL is high. */
static void i2c_start(void)
{
    sda_high();
    scl_high();
    half_period();
    sda_low();
    half_period();
    scl_low();
    half_period();
}

/** Generate STOP condition: SDA low→high while SCL is high. */
static void i2c_stop(void)
{
    sda_low();
    half_period();
    scl_high();
    half_period();
    sda_high();
    half_period();
}

/**
 * @brief Transmit one byte MSB-first, read ACK/NACK.
 *
 * @param byte  The byte to transmit.
 * @param flags Error injection flags.
 * @return true if target ACKed (SDA low during 9th clock).
 */
static bool i2c_write_byte(uint8_t byte, uint8_t flags)
{
    /* 8 data bits, MSB first */
    for (int i = 7; i >= 0; i--) {
        if (byte & (1u << i)) {
            sda_high();
        } else {
            sda_low();
        }
        half_period();

        scl_high();

        /* Optional clock stretching abuse */
        if (flags & FUZZ_I2C_FLAG_CLOCK_STRETCH) {
            uint32_t extra = (prng_next() % 5u) * s_half_period_us;
            if (extra > 0) sleep_us(extra);
        }

        half_period();
        scl_low();
        half_period();
    }

    /* 9th clock: ACK/NACK — release SDA, read target response */
    sda_high();          /* release SDA so target can pull it low for ACK */
    half_period();
    scl_high();
    half_period();

    bool ack = !sda_read();  /* ACK = SDA low */

    scl_low();
    half_period();

    return ack;
}

/* ── Public API ─────────────────────────────────────────────────── */

void fuzz_i2c_tx_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t speed_hz)
{
    s_sda_pin = sda_pin;
    s_scl_pin = scl_pin;

    /* Half period = 1 / (2 * frequency).  Clamp to minimum 1 µs. */
    s_half_period_us = (speed_hz > 0) ? (500000u / speed_hz) : 5u;
    if (s_half_period_us == 0) s_half_period_us = 1;

    s_prng = 0xDEAD5678u ^ (uint32_t)sda_pin ^ speed_hz;

    /* Configure both pins as inputs with pull-ups (idle high). */
    gpio_init(sda_pin);
    gpio_init(scl_pin);
    gpio_set_dir(sda_pin, GPIO_IN);
    gpio_set_dir(scl_pin, GPIO_IN);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    gpio_put(sda_pin, 0);  /* pre-load output register for open-drain low */
    gpio_put(scl_pin, 0);
}

void fuzz_i2c_tx_deinit(void)
{
    /* Release both lines to input */
    gpio_set_dir(s_sda_pin, GPIO_IN);
    gpio_set_dir(s_scl_pin, GPIO_IN);
    gpio_disable_pulls(s_sda_pin);
    gpio_disable_pulls(s_scl_pin);
}

bool fuzz_i2c_tx_send(const uint8_t *data, size_t len, uint8_t flags)
{
    if (len == 0) return true;

    i2c_start();

    bool all_acked = true;
    for (size_t i = 0; i < len; i++) {
        bool ack = i2c_write_byte(data[i], flags);
        if (!ack) {
            all_acked = false;
            if (!(flags & FUZZ_I2C_FLAG_SKIP_ACK)) {
                /* Normal behaviour: abort on NACK */
                i2c_stop();
                return false;
            }
            /* SKIP_ACK: ignore NACK and keep going */
        }
    }

    /* End of transaction */
    if (flags & FUZZ_I2C_FLAG_REPEATED_START) {
        i2c_start();  /* repeated START instead of STOP */
    } else if (!(flags & FUZZ_I2C_FLAG_NO_STOP)) {
        i2c_stop();
    }
    /* NO_STOP: leave bus in indeterminate state — stress test */

    return all_acked;
}
