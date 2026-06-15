/**
 * @file fuzz_worker.c
 * @brief Core 1 worker task for fuzz TX.
 *
 * Runs independently on Core 1 to avoid blocking capture on Core 0.
 * Integrates:
 * - Fuzzer orchestrator (corpus + mutations)
 * - Fuzz engine (stimulus queue management)
 * - TX transmission (UART/I2C)
 */

#include "fuzz_worker.h"
#include "fuzz_engine.h"
#include "fuzz_uart_tx.h"
#include "fuzz_i2c_tx.h"
#include "fuzzer_orchestrator.h"
#include "session.h"
#include "protocol/protocol.h"
#include "pico/stdlib.h"

/* The desktop already owns corpus selection for this workflow.
 * Keep the Core 1 built-in corpus generator disabled unless we
 * explicitly want standalone autonomous fuzzing.
 */
#define ENABLE_INTERNAL_FUZZ_CORPUS 0

/* Global orchestrator instance (persistent across Core 1 loop iterations) */
static fuzzer_orchestrator_t g_orchestrator;
static bool g_orchestrator_initialized = false;

/**
 * @brief Initialize orchestrator on first run.
 */
static void fuzz_worker_init_orchestrator(void) {
    if (g_orchestrator_initialized) {
        return;
    }
    
    /* Initialize with default settings:
     * - RNG seed: 0xDEADBEEF (reproducible)
     * - Mutations: All types enabled
     * - Rate: 70% chance of mutation per stimulus
     * - Max iterations: 0 (unlimited)
     */
    fuzzer_orchestrator_init(&g_orchestrator,
                             0xDEADBEEF,
                             FUZZER_MUTATE_ALL,
                             0.7f,
                             0);
    g_orchestrator_initialized = true;
}

/**
 * @brief Main worker loop for Core 1.
 *
 * Monitors fuzz_mode flag and:
 * 1. Orchestrates corpus iteration and mutation generation
 * 2. Processes fuzz engine (dequeues and transmits)
 *
 * Transmits via UART or I2C based on active_bus setting.
 */
void fuzz_worker_task(void) {
    fuzz_worker_init_orchestrator();
    
    while (1) {
        /* Check if fuzzing is enabled */
        if (!g_session.fuzz_mode || 
            g_session.current_state != HW_PROTOCOL_STATE_RUNNING) {
            
            /* Stop orchestrator if transitioning out of RUNNING */
            if (g_orchestrator.active) {
                fuzzer_orchestrator_stop(&g_orchestrator);
            }
            
            /* Sleep briefly to avoid busy-loop */
            sleep_us(100);
            continue;
        }
        
        /* Desktop-driven fuzzing: only service the queue that was already
         * populated by QUEUE_STIMULUS frames. Do not add a second autonomous
         * corpus stream here, otherwise the transmitted bytes no longer match
         * the GUI list.
         */
#if ENABLE_INTERNAL_FUZZ_CORPUS
        if (!g_orchestrator.active) {
            fuzzer_orchestrator_start(&g_orchestrator);
        }

        fuzzer_orchestrator_task(&g_orchestrator);
#endif
        
        /* Process fuzz engine (dequeues and transmits) */
        fuzz_engine_task();
        
        /* Brief sleep between iterations for efficiency */
        sleep_us(10);
    }
}
