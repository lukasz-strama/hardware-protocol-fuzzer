/**
 * @file fuzzer_corpus.h
 * @brief Corpus management for fuzzing.
 *
 * Manages a collection of seed inputs (corpus) that serve as the basis
 * for fuzzing mutations. The corpus is pre-compiled into firmware as
 * a C array; this module provides iteration and tracking.
 *
 * A corpus entry is simply a byte sequence with optional metadata.
 */

#ifndef FUZZER_CORPUS_H
#define FUZZER_CORPUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ─────────────────────────────────────────────────────────────────┐
 * CORPUS ENTRY STRUCTURE
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief A single corpus entry (seed input).
 */
typedef struct {
    const uint8_t *data;      /**< Pointer to seed data. */
    uint16_t len;             /**< Length in bytes. */
    uint16_t id;              /**< Unique corpus ID (hash or index). */
} fuzzer_corpus_entry_t;

/* ─────────────────────────────────────────────────────────────────┐
 * CORPUS ITERATOR
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Opaque corpus iterator state.
 *
 * Maintains current position for iteration, coverage tracking,
 * and statistics.
 */
typedef struct {
    uint32_t index;              /**< Current corpus index. */
    uint32_t total_entries;      /**< Total number of entries. */
    uint32_t iterations;         /**< Number of full corpus traversals. */
    uint32_t total_stimuli_sent; /**< Cumulative mutations queued. */
} fuzzer_corpus_iterator_t;

/* ─────────────────────────────────────────────────────────────────┐
 * PUBLIC API
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Initialize the corpus iterator.
 *
 * Must be called once before iteration begins.
 *
 * @param iter Pointer to iterator state (allocated by caller).
 */
void fuzzer_corpus_init(fuzzer_corpus_iterator_t *iter);

/**
 * @brief Get the next corpus entry (round-robin).
 *
 * @param iter Pointer to iterator state.
 * @param out_entry Pointer to fuzzer_corpus_entry_t (filled by this call).
 * @return true if an entry was retrieved; false if corpus is empty.
 */
bool fuzzer_corpus_next(fuzzer_corpus_iterator_t *iter,
                        fuzzer_corpus_entry_t *out_entry);

/**
 * @brief Get a specific corpus entry by index.
 *
 * @param index Zero-based corpus index.
 * @param out_entry Pointer to fuzzer_corpus_entry_t (filled by this call).
 * @return true if the entry exists; false if index is out of bounds.
 */
bool fuzzer_corpus_get(uint32_t index, fuzzer_corpus_entry_t *out_entry);

/**
 * @brief Get total number of corpus entries.
 *
 * @return Count of available seed inputs.
 */
uint32_t fuzzer_corpus_count(void);

/**
 * @brief Get current iteration statistics.
 *
 * @param iter Pointer to iterator state.
 * @return Total number of stimuli generated from this iterator.
 */
uint32_t fuzzer_corpus_total_generated(const fuzzer_corpus_iterator_t *iter);

/**
 * @brief Reset the iterator to the first corpus entry.
 *
 * @param iter Pointer to iterator state.
 */
void fuzzer_corpus_reset(fuzzer_corpus_iterator_t *iter);

/**
 * @brief Add built-in corpus entries (hardcoded test cases).
 *
 * This initializes a minimal default corpus useful for testing.
 * Can be replaced with custom corpus data at compile time.
 */
void fuzzer_corpus_init_builtin(void);

#endif /* FUZZER_CORPUS_H */
