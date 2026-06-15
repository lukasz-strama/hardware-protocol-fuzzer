/**
 * @file fuzz_engine.h
 * @brief Firmware-side fuzz engine: stimulus queue, policy, mutations.
 *
 * The desktop sends SET_FUZZ_POLICY and QUEUE_STIMULUS commands.
 * The engine stores up to FUZZ_MAX_ENTRIES stimuli in a ring queue
 * backed by a fixed-size byte pool.  During Running state,
 * fuzz_engine_task() picks the next stimulus, optionally mutates it,
 * and dispatches it to the appropriate bus transmitter (UART or I2C).
 */
#ifndef FUZZ_ENGINE_H
#define FUZZ_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_layout.h"

/* ── Policy flags (SET_FUZZ_POLICY.policy_flags) ────────────────── */

/** Data-level mutation: flip one random bit in payload. */
#define FUZZ_POLICY_BIT_FLIP         (1u << 0)
/** Data-level mutation: randomly shorten payload. */
#define FUZZ_POLICY_TRUNCATE         (1u << 1)
/** Physical-layer: insert wrong parity bit (UART only). */
#define FUZZ_POLICY_CORRUPT_PARITY   (1u << 2)
/** Physical-layer: drive stop bit low (UART only). */
#define FUZZ_POLICY_BAD_STOP_BIT     (1u << 3)
/** Physical-layer: random inter-byte timing (UART only). */
#define FUZZ_POLICY_TIMING_DISTORT   (1u << 4)
/** I2C protocol-level: skip ACK checks. */
#define FUZZ_POLICY_I2C_SKIP_ACK     (1u << 5)
/** I2C protocol-level: repeated START instead of STOP. */
#define FUZZ_POLICY_I2C_REPEATED_START (1u << 6)
/** I2C protocol-level: clock stretching abuse. */
#define FUZZ_POLICY_I2C_CLOCK_STRETCH  (1u << 7)

/* ── Queue limits ───────────────────────────────────────────────── */

#define FUZZ_MAX_ENTRIES  32u
#define FUZZ_POOL_SIZE    4096u
#define FUZZ_WORK_BUF     512u

/* ── Public API ─────────────────────────────────────────────────── */

/**
 * @brief Reset the engine to initial state (empty queue, no policy).
 */
void fuzz_engine_init(void);

/**
 * @brief Apply a fuzz policy from a SET_FUZZ_POLICY payload.
 *
 * Deserialises the payload with explicit byte decoding (no struct cast).
 * Validates limits against firmware maximums.
 *
 * @param payload  Raw payload bytes from the protocol frame.
 * @param len      Length of the payload.
 * @return true if the policy was accepted; false on validation error.
 */
bool fuzz_engine_set_policy(const uint8_t *payload, uint16_t len);

/**
 * @brief Enqueue a stimulus from a QUEUE_STIMULUS payload.
 *
 * Deserialises the fixed 8-byte header, copies inline data to the pool.
 * Rejects if queue is full or pool would overflow.
 *
 * @param payload    Raw payload bytes (8-byte header + inline data).
 * @param total_len  Total payload length.
 * @return true if the stimulus was queued; false on error.
 */
bool fuzz_engine_queue_stimulus(const uint8_t *payload, uint16_t total_len);

/**
 * @brief Start the fuzz engine (called after START_FUZZ).
 *
 * Initialises the appropriate bus transmitter based on g_session.active_bus.
 */
void fuzz_engine_start(void);

/**
 * @brief Stop the fuzz engine and release bus resources.
 */
void fuzz_engine_stop(void);

/**
 * @brief Main task, called every main-loop iteration while Running.
 *
 * Picks the next stimulus, applies mutations, transmits it, emits
 * a FUZZ_TX report frame, and manages queue lifecycle.
 */
void fuzz_engine_task(void);

/**
 * @brief Return the number of stimuli currently in the queue.
 */
uint8_t fuzz_engine_queued_count(void);

#endif /* FUZZ_ENGINE_H */
