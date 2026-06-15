/**
 * @file fuzzer_orchestrator.c
 * @brief Fuzzer orchestrator implementation.
 *
 * Coordinates corpus iteration, mutation, and stimulus queueing.
 */

#include "fuzzer_orchestrator.h"
#include "fuzz_engine.h"
#include "session.h"
#include "protocol_layout.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────┐
 * ORCHESTRATOR TASK LOGIC
 * ─────────────────────────────────────────────────────────────────┘ */

/**
 * @brief Helper: Convert a corpus entry + mutations into a QUEUE_STIMULUS payload.
 *
 * Returns false if the payload would exceed FUZZ_WORK_BUF or other limits.
 */
static bool build_stimulus_payload(const fuzzer_corpus_entry_t *entry,
                                   uint16_t mutated_len,
                                   uint8_t *payload, uint16_t *payload_len) {
    if (!entry || !payload || !payload_len) {
        return false;
    }
    
    /* QUEUE_STIMULUS payload format:
     *   Offset 0-7:   8-byte header (flags, reserved, etc.)
     *   Offset 8+:    Inline stimulus data
     *
     * We assume a simple format: [flags(1), reserved(7), data...]
     */
    
    uint16_t max_payload = 512; /* Should match protocol limits */
    
    if (8 + mutated_len > max_payload) {
        return false;
    }
    
    /* Build 8-byte header */
    payload[0] = 0x00; /* flags: normal stimulus */
    memset(&payload[1], 0x00, 7); /* reserved */
    
    /* Copy mutated data */
    memcpy(&payload[8], entry->data, mutated_len);
    
    *payload_len = 8 + mutated_len;
    return true;
}

/**
 * @brief Queue a single stimulus via fuzz_engine.
 */
static bool queue_stimulus(fuzzer_orchestrator_t *orch,
                           const fuzzer_corpus_entry_t *entry,
                           uint8_t *mutated_data, uint16_t mutated_len) {
    if (!orch || !entry) {
        return false;
    }
    
    /* Build payload (header + data) */
    uint8_t payload[512];
    uint16_t payload_len;
    
    if (!build_stimulus_payload(entry, mutated_len, payload, &payload_len)) {
        return false;
    }
    
    /* Attempt to queue via fuzz_engine */
    if (!fuzz_engine_queue_stimulus(payload, payload_len)) {
        orch->queue_full_drops++;
        return false;
    }
    
    orch->total_queued++;
    return true;
}

/* ─────────────────────────────────────────────────────────────────┐
 * PUBLIC API IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────┘ */

void fuzzer_orchestrator_init(fuzzer_orchestrator_t *orch,
                              uint32_t rng_seed,
                              uint32_t mutation_types,
                              float mutation_rate,
                              uint32_t max_iterations) {
    if (!orch) return;
    
    /* Initialize corpus iterator */
    fuzzer_corpus_init(&orch->corpus_iter);
    fuzzer_corpus_init_builtin();
    
    /* Initialize mutation engine */
    fuzzer_mutator_init(&orch->mutator, rng_seed, mutation_types, mutation_rate);
    
    /* Initialize orchestrator state */
    orch->total_mutations = 0;
    orch->total_queued = 0;
    orch->seed_counter = 0;
    orch->max_iterations = max_iterations;
    orch->active = false;
    orch->paused = false;
    orch->anomalies_detected = 0;
    orch->queue_full_drops = 0;
}

void fuzzer_orchestrator_start(fuzzer_orchestrator_t *orch) {
    if (!orch) return;
    orch->active = true;
    orch->paused = false;
}

void fuzzer_orchestrator_stop(fuzzer_orchestrator_t *orch) {
    if (!orch) return;
    orch->active = false;
    orch->paused = false;
}

void fuzzer_orchestrator_pause(fuzzer_orchestrator_t *orch) {
    if (!orch) return;
    orch->paused = true;
}

void fuzzer_orchestrator_resume(fuzzer_orchestrator_t *orch) {
    if (!orch) return;
    orch->paused = false;
}

bool fuzzer_orchestrator_task(fuzzer_orchestrator_t *orch) {
    if (!orch || !orch->active || orch->paused) {
        return false;
    }
    
    /* Check if session is in RUNNING state */
    if (g_session.current_state != HW_PROTOCOL_STATE_RUNNING) {
        return false;
    }
    
    /* Check if we should limit iterations */
    if (orch->max_iterations > 0 &&
        orch->corpus_iter.total_stimuli_sent >= orch->max_iterations) {
        orch->active = false;
        return false;
    }
    
    /* Get next corpus entry */
    fuzzer_corpus_entry_t entry;
    if (!fuzzer_corpus_next(&orch->corpus_iter, &entry)) {
        /* Corpus empty or error */
        orch->active = false;
        return false;
    }
    
    /* Apply mutations */
    uint8_t mutated[512];
    uint16_t mutated_len;
    
    if (!fuzzer_mutator_apply(&orch->mutator,
                              entry.data, entry.len,
                              mutated, sizeof(mutated),
                              &mutated_len)) {
        return false;
    }
    
    orch->total_mutations++;
    
    /* Update corpus entry with mutated data */
    fuzzer_corpus_entry_t mutated_entry = entry;
    mutated_entry.data = mutated;
    mutated_entry.len = mutated_len;
    
    /* Queue the stimulus */
    if (queue_stimulus(orch, &mutated_entry, mutated, mutated_len)) {
        orch->seed_counter++;
    }
    
    return true;
}

void fuzzer_orchestrator_get_stats(const fuzzer_orchestrator_t *orch,
                                   uint32_t *out_total,
                                   uint32_t *out_iterations,
                                   uint32_t *out_anomalies) {
    if (!orch) return;
    
    if (out_total) {
        *out_total = orch->total_queued;
    }
    if (out_iterations) {
        *out_iterations = orch->corpus_iter.iterations;
    }
    if (out_anomalies) {
        *out_anomalies = orch->anomalies_detected;
    }
}

void fuzzer_orchestrator_reset_stats(fuzzer_orchestrator_t *orch) {
    if (!orch) return;
    
    orch->total_mutations = 0;
    orch->total_queued = 0;
    orch->seed_counter = 0;
    orch->anomalies_detected = 0;
    orch->queue_full_drops = 0;
    
    fuzzer_corpus_reset(&orch->corpus_iter);
}
