/**
 * @file fuzzer_corpus.c
 * @brief Corpus management implementation.
 */

#include "fuzzer_corpus.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────┐
 * BUILT-IN CORPUS DATA
 *
 * These are seed inputs compiled directly into firmware.
 * Each entry is a byte sequence for I2C or UART fuzzing.
 * ─────────────────────────────────────────────────────────────────┘ */

/* UART seed patterns */
static const uint8_t corpus_uart_data_0[] = {0xAA, 0xBB, 0xCC, 0xDD};
static const uint8_t corpus_uart_data_1[] = {0xFF, 0x00, 0xFF, 0x00};
static const uint8_t corpus_uart_data_2[] = {0x55, 0xAA, 0x55, 0xAA};
static const uint8_t corpus_uart_data_3[] = {0xDE, 0xAD, 0xBE, 0xEF};
static const uint8_t corpus_uart_data_4[] = {0x00};
static const uint8_t corpus_uart_data_5[] = {0xFF};

/* I2C address + data patterns */
static const uint8_t corpus_i2c_data_0[] = {0x42, 0xAA, 0xBB, 0xCC};  /* addr + 3 bytes */
static const uint8_t corpus_i2c_data_1[] = {0x68, 0x00};               /* addr + 1 byte */
static const uint8_t corpus_i2c_data_2[] = {0x7F, 0xFF, 0xFF, 0xFF};  /* addr + 3 bytes */
static const uint8_t corpus_i2c_data_3[] = {0x50};                    /* single address (read) */

/* Array of corpus entries */
static const fuzzer_corpus_entry_t g_corpus[] = {
    /* UART entries */
    {.data = corpus_uart_data_0, .len = sizeof(corpus_uart_data_0), .id = 0x0001},
    {.data = corpus_uart_data_1, .len = sizeof(corpus_uart_data_1), .id = 0x0002},
    {.data = corpus_uart_data_2, .len = sizeof(corpus_uart_data_2), .id = 0x0003},
    {.data = corpus_uart_data_3, .len = sizeof(corpus_uart_data_3), .id = 0x0004},
    {.data = corpus_uart_data_4, .len = sizeof(corpus_uart_data_4), .id = 0x0005},
    {.data = corpus_uart_data_5, .len = sizeof(corpus_uart_data_5), .id = 0x0006},
    
    /* I2C entries */
    {.data = corpus_i2c_data_0, .len = sizeof(corpus_i2c_data_0), .id = 0x0101},
    {.data = corpus_i2c_data_1, .len = sizeof(corpus_i2c_data_1), .id = 0x0102},
    {.data = corpus_i2c_data_2, .len = sizeof(corpus_i2c_data_2), .id = 0x0103},
    {.data = corpus_i2c_data_3, .len = sizeof(corpus_i2c_data_3), .id = 0x0104},
};

static const uint32_t g_corpus_count = sizeof(g_corpus) / sizeof(g_corpus[0]);

/* ─────────────────────────────────────────────────────────────────┐
 * IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────┘ */

void fuzzer_corpus_init(fuzzer_corpus_iterator_t *iter) {
    if (!iter) return;
    iter->index = 0;
    iter->total_entries = g_corpus_count;
    iter->iterations = 0;
    iter->total_stimuli_sent = 0;
}

bool fuzzer_corpus_next(fuzzer_corpus_iterator_t *iter,
                        fuzzer_corpus_entry_t *out_entry) {
    if (!iter || !out_entry || g_corpus_count == 0) {
        return false;
    }
    
    /* Get current entry */
    *out_entry = g_corpus[iter->index];
    
    /* Advance to next */
    iter->index++;
    if (iter->index >= g_corpus_count) {
        iter->index = 0;
        iter->iterations++;
    }
    
    iter->total_stimuli_sent++;
    return true;
}

bool fuzzer_corpus_get(uint32_t index, fuzzer_corpus_entry_t *out_entry) {
    if (!out_entry || index >= g_corpus_count) {
        return false;
    }
    
    *out_entry = g_corpus[index];
    return true;
}

uint32_t fuzzer_corpus_count(void) {
    return g_corpus_count;
}

uint32_t fuzzer_corpus_total_generated(const fuzzer_corpus_iterator_t *iter) {
    if (!iter) return 0;
    return iter->total_stimuli_sent;
}

void fuzzer_corpus_reset(fuzzer_corpus_iterator_t *iter) {
    if (!iter) return;
    iter->index = 0;
    iter->iterations = 0;
    /* Note: we don't reset total_stimuli_sent for lifetime statistics */
}

void fuzzer_corpus_init_builtin(void) {
    /* Built-in corpus is statically initialized */
}
