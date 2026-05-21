#ifndef FUZZ_ENGINE_H
#define FUZZ_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_layout.h"

/*
 * Firmware-side policy_flags bits (SET_FUZZ_POLICY.policy_flags).
 * Desktop sets these to control which error classes the engine injects.
 */
#define FUZZ_POLICY_CORRUPT_PARITY  (1u << 0)  /* insert wrong parity bit */
#define FUZZ_POLICY_TIMING_DISTORT  (1u << 1)  /* randomise inter-byte gaps */
#define FUZZ_POLICY_BAD_STOP_BIT    (1u << 2)  /* drive stop bit low */
#define FUZZ_POLICY_BIT_FLIP        (1u << 3)  /* flip one random data bit */
#define FUZZ_POLICY_TRUNCATE        (1u << 4)  /* randomly shorten payload */

void    fuzz_engine_init(void);
bool    fuzz_engine_set_policy(const uint8_t *payload, uint16_t len);
bool    fuzz_engine_queue_stimulus(const uint8_t *payload, uint16_t total_len);
void    fuzz_engine_start(void);
void    fuzz_engine_stop(void);
void    fuzz_engine_task(void);
uint8_t fuzz_engine_queued_count(void);

#endif
