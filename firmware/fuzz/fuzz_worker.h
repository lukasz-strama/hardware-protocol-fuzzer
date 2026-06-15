/**
 * @file fuzz_worker.h
 * @brief Core 1 worker task for fuzz TX.
 *
 * Runs independently on Core 1 to avoid blocking capture on Core 0.
 * Integrates orchestrator (corpus + mutations) with fuzz engine (TX).
 */

#ifndef FUZZ_WORKER_H
#define FUZZ_WORKER_H

/**
 * @brief Main worker loop for Core 1.
 *
 * This function should be launched on Core 1 via:
 *   multicore_launch_core1(fuzz_worker_task);
 *
 * It runs indefinitely, coordinating:
 * 1. Corpus iteration and mutation generation (via orchestrator)
 * 2. Stimulus transmission (via fuzz engine)
 *
 * Runs only while session is in RUNNING state and fuzz_mode is enabled.
 */
void fuzz_worker_task(void);

#endif /* FUZZ_WORKER_H */
