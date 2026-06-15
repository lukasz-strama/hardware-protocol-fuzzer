/**
 * @file fuzzer_orchestrator.h
 * @brief Fuzzer orchestration and coordination.
 *
 * Coordinates corpus iteration, mutation application, and stimulus queueing.
 * Tracks coverage, anomalies, and fuzz statistics.
 *
 * Designed to run on Core 1 alongside fuzz_engine_task().
 * Communicates with Core 0 via shared session state.
 */

#ifndef FUZZER_ORCHESTRATOR_H
#define FUZZER_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "fuzzer_corpus.h"
#include "fuzzer_mutator.h"

/* ─────────────────────────────────────────────────────────────────┐
 * ORCHESTRATOR STATE
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Fuzzer orchestrator state and statistics.
 *
 * Manages overall fuzz campaign lifecycle, corpus iteration,
 * and coverage tracking.
 */
typedef struct {
    fuzzer_corpus_iterator_t corpus_iter;    /**< Current corpus position. */
    fuzzer_mutator_t mutator;                /**< Mutation engine. */
    
    uint32_t total_mutations;                /**< Total mutations applied. */
    uint32_t total_queued;                   /**< Total stimuli queued. */
    
    uint32_t seed_counter;                   /**< Current seed being fuzzed. */
    uint32_t max_iterations;                 /**< Max mutations per seed (0=unlimited). */
    
    bool active;                             /**< Orchestrator is running. */
    bool paused;                             /**< Orchestrator is paused. */
    
    /* Statistics */
    uint32_t anomalies_detected;             /**< Unusual behaviors. */
    uint32_t queue_full_drops;               /**< Dropped due to queue full. */
} fuzzer_orchestrator_t;

/* ─────────────────────────────────────────────────────────────────┐
 * PUBLIC API
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Initialize the fuzzer orchestrator.
 *
 * Sets up corpus, mutation engine, and initial state.
 *
 * @param orch Pointer to orchestrator state.
 * @param rng_seed RNG seed for reproducible fuzzing.
 * @param mutation_types Bitmask of enabled mutations.
 * @param mutation_rate Probability [0.0, 1.0] of applying mutation.
 * @param max_iterations Max mutations per corpus entry (0 = unlimited).
 */
void fuzzer_orchestrator_init(fuzzer_orchestrator_t *orch,
                              uint32_t rng_seed,
                              uint32_t mutation_types,
                              float mutation_rate,
                              uint32_t max_iterations);

/**
 * @brief Start the fuzzer.
 *
 * Begins corpus iteration and stimulus generation.
 * Should be called when fuzzing begins.
 *
 * @param orch Pointer to orchestrator state.
 */
void fuzzer_orchestrator_start(fuzzer_orchestrator_t *orch);

/**
 * @brief Stop the fuzzer.
 *
 * Halts stimulus generation and queuing.
 *
 * @param orch Pointer to orchestrator state.
 */
void fuzzer_orchestrator_stop(fuzzer_orchestrator_t *orch);

/**
 * @brief Pause the fuzzer (can be resumed).
 *
 * @param orch Pointer to orchestrator state.
 */
void fuzzer_orchestrator_pause(fuzzer_orchestrator_t *orch);

/**
 * @brief Resume the fuzzer after pause.
 *
 * @param orch Pointer to orchestrator state.
 */
void fuzzer_orchestrator_resume(fuzzer_orchestrator_t *orch);

/**
 * @brief Main orchestrator task (call periodically).
 *
 * Generates mutations, queues stimuli to fuzz_engine.
 * Should be called from Core 1 main loop when fuzzing.
 *
 * @param orch Pointer to orchestrator state.
 * @return true if work was performed; false if paused/stopped.
 */
bool fuzzer_orchestrator_task(fuzzer_orchestrator_t *orch);

/**
 * @brief Get current statistics.
 *
 * @param orch Pointer to orchestrator state.
 * @param out_total Pointer to uint32_t (filled with total stimuli queued).
 * @param out_iterations Pointer to uint32_t (filled with full corpus iterations).
 * @param out_anomalies Pointer to uint32_t (filled with anomalies detected).
 */
void fuzzer_orchestrator_get_stats(const fuzzer_orchestrator_t *orch,
                                   uint32_t *out_total,
                                   uint32_t *out_iterations,
                                   uint32_t *out_anomalies);

/**
 * @brief Reset statistics without stopping the fuzzer.
 *
 * @param orch Pointer to orchestrator state.
 */
void fuzzer_orchestrator_reset_stats(fuzzer_orchestrator_t *orch);

#endif /* FUZZER_ORCHESTRATOR_H */
