/**
 * @file fuzz_i2c_tx.h
 * @brief GPIO bit-bang I2C master transmitter for fuzz stimulus injection.
 *
 * Uses raw GPIO in open-drain mode (not the hardware I2C peripheral)
 * so that we can deliberately inject protocol-level errors:
 *  - skipping ACK checks,
 *  - repeated STARTs without STOP,
 *  - clock stretching abuse,
 *  - wrong address bytes.
 */
#ifndef FUZZ_I2C_TX_H
#define FUZZ_I2C_TX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Send raw bytes without checking ACK from target. */
#define FUZZ_I2C_FLAG_SKIP_ACK       (1u << 0)
/** Issue repeated START instead of STOP at end of frame. */
#define FUZZ_I2C_FLAG_REPEATED_START (1u << 1)
/** Hold SCL low for extra time (clock stretching abuse). */
#define FUZZ_I2C_FLAG_CLOCK_STRETCH  (1u << 2)
/** Omit the STOP condition at end of transaction. */
#define FUZZ_I2C_FLAG_NO_STOP        (1u << 3)

/**
 * @brief Initialise the I2C TX pins for bit-bang master mode.
 *
 * Configures both SDA and SCL as open-drain GPIO outputs, idles high.
 *
 * @param sda_pin   GPIO number for SDA.
 * @param scl_pin   GPIO number for SCL.
 * @param speed_hz  Target clock frequency (e.g. 100000 for 100 kHz).
 */
void fuzz_i2c_tx_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t speed_hz);

/**
 * @brief Release the I2C TX pins back to input (high-Z).
 */
void fuzz_i2c_tx_deinit(void);

/**
 * @brief Transmit a buffer of raw bytes as an I2C master.
 *
 * The first byte is the address+R/W byte.  The remaining bytes are
 * data.  Error injection is controlled by @p flags.
 *
 * @param data   Pointer to bytes (address + payload).
 * @param len    Number of bytes including address.
 * @param flags  Bitmask of FUZZ_I2C_FLAG_* options.
 * @return true if all ACKs received (when not SKIP_ACK), false otherwise.
 */
bool fuzz_i2c_tx_send(const uint8_t *data, size_t len, uint8_t flags);

#endif /* FUZZ_I2C_TX_H */
