/**
 * @file fuzzer_mutator.h
 * @brief Mutation engine for fuzzing.
 *
 * Applies deterministic and pseudo-random mutations to corpus data.
 * Supports bit-level, byte-level, and protocol-aware mutations.
 *
 * Includes:
 * - Bit flip: XOR random bit with 1
 * - Byte corrupt: Replace random byte with random value
 * - Truncate: Shorten payload to random length
 * - Extend: Append random bytes
 * - Timing skew: (emulated in TX layer, not here)
 * - Protocol-aware: Frame structure preservation (future)
 */

#ifndef FUZZER_MUTATOR_H
#define FUZZER_MUTATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ─────────────────────────────────────────────────────────────────┐
 * MUTATION TYPES (Bitmask)
 * ─────────────────────────────────────────────────────────────────┘ */

#define FUZZER_MUTATE_BIT_FLIP       (1u << 0)  /**< Flip single random bit */
#define FUZZER_MUTATE_BYTE_CORRUPT   (1u << 1)  /**< Replace random byte */
#define FUZZER_MUTATE_TRUNCATE       (1u << 2)  /**< Shorten to random len */
#define FUZZER_MUTATE_EXTEND         (1u << 3)  /**< Append random bytes */
#define FUZZER_MUTATE_REPEAT_BYTE    (1u << 4)  /**< Duplicate random byte */
#define FUZZER_MUTATE_SWAP_BYTES     (1u << 5)  /**< Swap two random bytes */

#define FUZZER_MUTATE_ALL            (0x3Fu)    /**< All mutations enabled */

/* ─────────────────────────────────────────────────────────────────┐
 * MUTATOR STATE
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Linear congruential generator (LCG) for deterministic RNG.
 *
 * Provides reproducible fuzzing with a fixed seed.
 * Simpler and faster than external RNG libraries.
 */
typedef struct {
    uint32_t state;           /**< Current LCG state. */
    uint32_t mutation_types;  /**< Bitmask of enabled mutations. */
    float mutation_rate;      /**< Probability [0.0, 1.0] of applying mutation. */
} fuzzer_mutator_t;

/* ─────────────────────────────────────────────────────────────────┐
 * PUBLIC API
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Initialize the mutation engine.
 *
 * @param mutator Pointer to mutator state.
 * @param seed Initial RNG seed (for reproducibility).
 * @param mutation_types Bitmask of enabled mutation types.
 * @param mutation_rate Probability [0.0, 1.0] of applying a mutation.
 */
void fuzzer_mutator_init(fuzzer_mutator_t *mutator, uint32_t seed,
                         uint32_t mutation_types, float mutation_rate);

/**
 * @brief Apply mutations to a copy of input data.
 *
 * Reads from `input`, applies zero or more mutations based on
 * the mutator policy, and writes to `output`.
 *
 * @param mutator Pointer to mutator state.
 * @param input Input data (unchanged).
 * @param input_len Length of input.
 * @param output Buffer for mutated data (must be at least input_len + 64).
 * @param max_output_len Maximum output length allowed.
 * @param out_len Pointer to uint16_t (filled with output length).
 * @return true if mutation succeeded; false if output buffer too small.
 */
bool fuzzer_mutator_apply(fuzzer_mutator_t *mutator,
                         const uint8_t *input, uint16_t input_len,
                         uint8_t *output, uint16_t max_output_len,
                         uint16_t *out_len);

/**
 * @brief Get the next pseudo-random 32-bit value.
 *
 * Uses internal LCG state. Updates the mutator state.
 *
 * @param mutator Pointer to mutator state.
 * @return Next random value.
 */
uint32_t fuzzer_mutator_rand(fuzzer_mutator_t *mutator);

/**
 * @brief Get a random byte [0, 255].
 *
 * Convenience wrapper around fuzzer_mutator_rand().
 *
 * @param mutator Pointer to mutator state.
 * @return Random byte.
 */
uint8_t fuzzer_mutator_rand_byte(fuzzer_mutator_t *mutator);

/**
 * @brief Get a random value in range [0, max).
 *
 * @param mutator Pointer to mutator state.
 * @param max Upper bound (exclusive).
 * @return Random value in [0, max).
 */
uint32_t fuzzer_mutator_rand_range(fuzzer_mutator_t *mutator, uint32_t max);

#endif /* FUZZER_MUTATOR_H */
