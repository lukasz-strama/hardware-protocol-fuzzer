/**
 * @file fuzzer_mutator.c
 * @brief Mutation engine implementation.
 */

#include "fuzzer_mutator.h"
#include <string.h>
#include <math.h>

/* ─────────────────────────────────────────────────────────────────┐
 * LINEAR CONGRUENTIAL GENERATOR (LCG)
 * ─────────────────────────────────────────────────────────────────┘ */

/* LCG parameters (glibc style) */
#define LCG_MULTIPLIER 1103515245u
#define LCG_INCREMENT   12345u
#define LCG_MODULUS     (1u << 31)

/**
 * @brief Perform one LCG step.
 */
static uint32_t lcg_next(uint32_t *state) {
    *state = (LCG_MULTIPLIER * *state + LCG_INCREMENT) % LCG_MODULUS;
    return *state >> 1;  /* Use upper bits */
}

/* ─────────────────────────────────────────────────────────────────┐
 * MUTATION HELPERS
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Flip a single random bit in data.
 */
static void mutate_bit_flip(fuzzer_mutator_t *mutator,
                            uint8_t *data, uint16_t len) {
    if (len == 0) return;
    
    uint32_t byte_idx = fuzzer_mutator_rand_range(mutator, len);
    uint32_t bit_idx = fuzzer_mutator_rand_range(mutator, 8);
    
    data[byte_idx] ^= (1u << bit_idx);
}

/**
 * @brief Replace a random byte with a random value.
 */
static void mutate_byte_corrupt(fuzzer_mutator_t *mutator,
                                uint8_t *data, uint16_t len) {
    if (len == 0) return;
    
    uint32_t idx = fuzzer_mutator_rand_range(mutator, len);
    data[idx] = fuzzer_mutator_rand_byte(mutator);
}

/**
 * @brief Shorten data to random length.
 */
static uint16_t mutate_truncate(fuzzer_mutator_t *mutator,
                                uint8_t *data, uint16_t len) {
    if (len == 0) return 0;
    
    uint32_t new_len = fuzzer_mutator_rand_range(mutator, len);
    return (uint16_t)new_len;
}

/**
 * @brief Append random bytes.
 */
static uint16_t mutate_extend(fuzzer_mutator_t *mutator,
                              uint8_t *data, uint16_t len,
                              uint16_t max_len) {
    if (len >= max_len) return len;
    
    /* Append 1-16 random bytes */
    uint32_t extra = fuzzer_mutator_rand_range(mutator, 16) + 1;
    uint16_t new_len = len + (uint16_t)extra;
    
    if (new_len > max_len) {
        new_len = max_len;
    }
    
    for (uint16_t i = len; i < new_len; i++) {
        data[i] = fuzzer_mutator_rand_byte(mutator);
    }
    
    return new_len;
}

/**
 * @brief Duplicate a random byte.
 */
static uint16_t mutate_repeat_byte(fuzzer_mutator_t *mutator,
                                   uint8_t *data, uint16_t len,
                                   uint16_t max_len) {
    if (len == 0 || len >= max_len) return len;
    
    /* Pick a random byte and duplicate it at a random position */
    uint32_t byte_idx = fuzzer_mutator_rand_range(mutator, len);
    uint8_t byte_val = data[byte_idx];
    
    /* Insert position (after the byte) */
    uint32_t insert_pos = fuzzer_mutator_rand_range(mutator, len + 1);
    
    /* Shift and insert */
    memmove(&data[insert_pos + 1], &data[insert_pos], len - insert_pos);
    data[insert_pos] = byte_val;
    
    return len + 1;
}

/**
 * @brief Swap two random bytes.
 */
static void mutate_swap_bytes(fuzzer_mutator_t *mutator,
                              uint8_t *data, uint16_t len) {
    if (len < 2) return;
    
    uint32_t idx1 = fuzzer_mutator_rand_range(mutator, len);
    uint32_t idx2 = fuzzer_mutator_rand_range(mutator, len);
    
    if (idx1 != idx2) {
        uint8_t tmp = data[idx1];
        data[idx1] = data[idx2];
        data[idx2] = tmp;
    }
}

/* ─────────────────────────────────────────────────────────────────┐
 * PUBLIC API IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────┘ */

void fuzzer_mutator_init(fuzzer_mutator_t *mutator, uint32_t seed,
                         uint32_t mutation_types, float mutation_rate) {
    if (!mutator) return;
    
    mutator->state = seed ? seed : 0xDEADBEEF;
    mutator->mutation_types = mutation_types;
    mutator->mutation_rate = mutation_rate;
    
    /* Clamp rate to [0.0, 1.0] */
    if (mutator->mutation_rate < 0.0f) {
        mutator->mutation_rate = 0.0f;
    } else if (mutator->mutation_rate > 1.0f) {
        mutator->mutation_rate = 1.0f;
    }
}

uint32_t fuzzer_mutator_rand(fuzzer_mutator_t *mutator) {
    if (!mutator) return 0;
    return lcg_next(&mutator->state);
}

uint8_t fuzzer_mutator_rand_byte(fuzzer_mutator_t *mutator) {
    return (uint8_t)(fuzzer_mutator_rand(mutator) & 0xFFu);
}

uint32_t fuzzer_mutator_rand_range(fuzzer_mutator_t *mutator, uint32_t max) {
    if (max == 0) return 0;
    return fuzzer_mutator_rand(mutator) % max;
}

bool fuzzer_mutator_apply(fuzzer_mutator_t *mutator,
                         const uint8_t *input, uint16_t input_len,
                         uint8_t *output, uint16_t max_output_len,
                         uint16_t *out_len) {
    if (!mutator || !input || !output || !out_len) {
        return false;
    }
    
    if (max_output_len < input_len) {
        return false;
    }
    
    /* Copy input to output */
    memcpy(output, input, input_len);
    uint16_t current_len = input_len;
    
    /* Check if we should mutate at all */
    uint32_t rand_val = fuzzer_mutator_rand(mutator);
    float rand_float = (float)rand_val / (float)0x7FFFFFFFu;
    
    if (rand_float > mutator->mutation_rate) {
        /* No mutation */
        *out_len = current_len;
        return true;
    }
    
    /* Apply enabled mutations */
    
    if ((mutator->mutation_types & FUZZER_MUTATE_BIT_FLIP) &&
        fuzzer_mutator_rand_range(mutator, 2) == 0) {
        mutate_bit_flip(mutator, output, current_len);
    }
    
    if ((mutator->mutation_types & FUZZER_MUTATE_BYTE_CORRUPT) &&
        fuzzer_mutator_rand_range(mutator, 2) == 0) {
        mutate_byte_corrupt(mutator, output, current_len);
    }
    
    if ((mutator->mutation_types & FUZZER_MUTATE_TRUNCATE) &&
        fuzzer_mutator_rand_range(mutator, 3) == 0) {
        current_len = mutate_truncate(mutator, output, current_len);
    }
    
    if ((mutator->mutation_types & FUZZER_MUTATE_EXTEND) &&
        fuzzer_mutator_rand_range(mutator, 3) == 0) {
        current_len = mutate_extend(mutator, output, current_len, max_output_len);
    }
    
    if ((mutator->mutation_types & FUZZER_MUTATE_REPEAT_BYTE) &&
        fuzzer_mutator_rand_range(mutator, 4) == 0) {
        if (current_len < max_output_len) {
            current_len = mutate_repeat_byte(mutator, output, current_len, max_output_len);
        }
    }
    
    if ((mutator->mutation_types & FUZZER_MUTATE_SWAP_BYTES) &&
        fuzzer_mutator_rand_range(mutator, 3) == 0) {
        mutate_swap_bytes(mutator, output, current_len);
    }
    
    *out_len = current_len;
    return true;
}
