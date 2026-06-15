/**
 * @file fuzz_engine.c
 * @brief Firmware-side fuzz engine implementation.
 *
 * Manages:
 *  - A ring queue of up to FUZZ_MAX_ENTRIES stimuli backed by a
 *    FUZZ_POOL_SIZE-byte memory pool.
 *  - Policy deserialisation and limit validation.
 *  - Stimulus selection (sequential, random, corpus-guided).
 *  - Data-level mutations (bit-flip, truncate).
 *  - Dispatch to UART or I2C bus transmitter.
 *  - FUZZ_TX report frame emission.
 *  - Time-budget enforcement.
 *
 * All payload deserialisation uses explicit byte-level decoding
 * (no struct casting) per contract requirements.
 */
#include "fuzz_engine.h"
#include "fuzz_uart_tx.h"
#include "fuzz_i2c_tx.h"
#include "session.h"
#include "protocol.h"
#include "protocol_handlers.h"
#include "pico/stdlib.h"
#include <string.h>

/* ── Internal types ─────────────────────────────────────────────── */

/** One stimulus entry in the ring queue. */
typedef struct {
    uint32_t stimulus_id;
    uint16_t pool_offset;     /**< Offset into the byte pool. */
    uint16_t data_len;
    uint8_t  stimulus_flags;
    uint8_t  stimulus_kind;
} fuzz_entry_t;

/** The stimulus ring queue. */
typedef struct {
    fuzz_entry_t entries[FUZZ_MAX_ENTRIES];
    uint8_t      pool[FUZZ_POOL_SIZE];    /**< Shared data pool. */
    uint16_t     pool_used;
    uint8_t      head;          /**< Index of oldest entry. */
    uint8_t      count;         /**< Number of entries in queue. */
    uint8_t      repeat_cursor; /**< For repeat mode. */
} fuzz_queue_t;

/** Cached policy from SET_FUZZ_POLICY. */
typedef struct {
    uint32_t time_budget_ms;
    uint16_t pending_bytes;
    uint8_t  policy_flags;
    uint8_t  selection_mode;
    uint8_t  repeat_mode;
    uint8_t  max_pending;
} fuzz_policy_t;

/* ── State ──────────────────────────────────────────────────────── */

static fuzz_queue_t  s_queue;
static fuzz_policy_t s_policy;
static bool          s_running;
static uint32_t      s_start_us;
static uint32_t      s_trace_seq;  /**< Sequence counter for FUZZ_TX frames. */

/* ── PRNG ───────────────────────────────────────────────────────── */

static uint32_t s_prng = 0x12345678u;

static uint32_t prng_next(void)
{
    s_prng ^= s_prng << 13;
    s_prng ^= s_prng >> 17;
    s_prng ^= s_prng << 5;
    return s_prng;
}

/* ── Data mutations ─────────────────────────────────────────────── */

static void mutate_bit_flip(uint8_t *buf, uint16_t len)
{
    if (len == 0) return;
    uint16_t byte_idx = (uint16_t)(prng_next() % len);
    uint8_t  bit_idx  = (uint8_t)(prng_next() % 8u);
    buf[byte_idx] ^= (1u << bit_idx);
}

static void mutate_truncate(uint16_t *len)
{
    if (*len <= 1) return;
    *len = (uint16_t)(1u + (prng_next() % (*len)));
}

/* ── FUZZ_TX emission ───────────────────────────────────────────── */

/**
 * @brief Emit a FUZZ_TX report frame to desktop.
 *
 * Wire layout (explicit byte encoding, no struct cast):
 *   stimulus_id : u32 LE
 *   trace_seq   : u32 LE
 *   mode        : u8
 *   flags       : u8
 *   data_len    : u16 LE
 *   data[]      : variable
 */
static void emit_fuzz_tx(uint32_t stimulus_id, const uint8_t *data,
                          uint16_t data_len, uint8_t flags)
{
    uint8_t buf[12 + FUZZ_WORK_BUF];
    uint32_t seq = s_trace_seq++;

    /* stimulus_id : u32 LE */
    buf[0] = (uint8_t)(stimulus_id & 0xFF);
    buf[1] = (uint8_t)((stimulus_id >>  8) & 0xFF);
    buf[2] = (uint8_t)((stimulus_id >> 16) & 0xFF);
    buf[3] = (uint8_t)((stimulus_id >> 24) & 0xFF);

    /* trace_seq : u32 LE */
    buf[4] = (uint8_t)(seq & 0xFF);
    buf[5] = (uint8_t)((seq >>  8) & 0xFF);
    buf[6] = (uint8_t)((seq >> 16) & 0xFF);
    buf[7] = (uint8_t)((seq >> 24) & 0xFF);

    /* mode : u8 */
    buf[8] = s_policy.selection_mode;

    /* flags : u8 */
    buf[9] = flags;

    /* data_len : u16 LE */
    buf[10] = (uint8_t)(data_len & 0xFF);
    buf[11] = (uint8_t)(data_len >> 8);

    /* data[] */
    if (data_len > 0 && data_len <= FUZZ_WORK_BUF) {
        memcpy(buf + 12, data, data_len);
    }

    protocol_send_frame(
        MSG_TYPE_FUZZ_TX,
        g_session.session_id,
        seq,
        buf,
        12u + data_len);
}

/* ── Public API ─────────────────────────────────────────────────── */

void fuzz_engine_init(void)
{
    memset(&s_queue, 0, sizeof(s_queue));
    memset(&s_policy, 0, sizeof(s_policy));
    s_running   = false;
    s_start_us  = 0;
    s_trace_seq = 0;
    s_prng      = 0x12345678u ^ time_us_32();
}

bool fuzz_engine_set_policy(const uint8_t *payload, uint16_t len)
{
    if (len < 12u) return false;

    /* Explicit byte-level deserialisation (no struct casting) */
    uint32_t time_budget_ms = (uint32_t)payload[0]
                            | ((uint32_t)payload[1] << 8)
                            | ((uint32_t)payload[2] << 16)
                            | ((uint32_t)payload[3] << 24);
    uint16_t pending_bytes  = (uint16_t)payload[4] | ((uint16_t)payload[5] << 8);
    uint8_t  policy_flags   = payload[6];
    uint8_t  selection_mode = payload[7];
    uint8_t  repeat_mode    = payload[8];
    uint8_t  max_pending    = payload[9];

    /* Validate against firmware limits */
    if (max_pending > FUZZ_MAX_ENTRIES || pending_bytes > FUZZ_POOL_SIZE) {
        return false;
    }

    /* Reset queue state — new policy means new session, clear stale data */
    memset(&s_queue, 0, sizeof(s_queue));

    s_policy.time_budget_ms = time_budget_ms;
    s_policy.pending_bytes  = pending_bytes;
    s_policy.policy_flags   = policy_flags;
    s_policy.selection_mode = selection_mode;
    s_policy.repeat_mode    = repeat_mode;
    s_policy.max_pending    = max_pending ? max_pending : (uint8_t)FUZZ_MAX_ENTRIES;
    return true;
}


bool fuzz_engine_queue_stimulus(const uint8_t *payload, uint16_t total_len)
{
    if (total_len < 8u) return false;  /* fixed header is 8 bytes */

    /* Explicit byte-level deserialisation */
    uint32_t stimulus_id    = (uint32_t)payload[0]
                            | ((uint32_t)payload[1] << 8)
                            | ((uint32_t)payload[2] << 16)
                            | ((uint32_t)payload[3] << 24);
    uint16_t data_len       = (uint16_t)payload[4] | ((uint16_t)payload[5] << 8);
    uint8_t  stimulus_flags = payload[6];
    uint8_t  stimulus_kind  = payload[7];
    const uint8_t *data     = payload + 8;

    /* Sanity: announced data_len must fit in payload */
    if ((uint16_t)(total_len - 8u) < data_len) return false;

    /* Queue capacity checks */
    if (s_queue.count >= s_policy.max_pending) return false;
    if ((uint16_t)(s_queue.pool_used + data_len) > FUZZ_POOL_SIZE) return false;

    uint8_t slot = (uint8_t)((s_queue.head + s_queue.count) % FUZZ_MAX_ENTRIES);
    fuzz_entry_t *e = &s_queue.entries[slot];
    e->stimulus_id    = stimulus_id;
    e->pool_offset    = s_queue.pool_used;
    e->data_len       = data_len;
    e->stimulus_flags = stimulus_flags;
    e->stimulus_kind  = stimulus_kind;

    if (data_len > 0) {
        memcpy(s_queue.pool + s_queue.pool_used, data, data_len);
    }
    s_queue.pool_used = (uint16_t)(s_queue.pool_used + data_len);
    s_queue.count++;
    return true;
}

void fuzz_engine_start(void)
{
    s_running  = true;
    s_start_us = time_us_32();
    s_queue.repeat_cursor = 0;

    /* Initialise the appropriate bus transmitter */
    if (g_session.active_bus == TARGET_BUS_UART) {
        fuzz_uart_tx_init(g_session.uart_tx_pin, g_session.uart_baudrate);
    } else if (g_session.active_bus == TARGET_BUS_I2C) {
        fuzz_i2c_tx_init(g_session.i2c_sda_pin, g_session.i2c_scl_pin,
                         g_session.i2c_frequency_khz * 1000u);
    }
}

void fuzz_engine_stop(void)
{
    if (!s_running) return;
    s_running = false;

    if (g_session.active_bus == TARGET_BUS_UART) {
        fuzz_uart_tx_deinit();
    } else if (g_session.active_bus == TARGET_BUS_I2C) {
        fuzz_i2c_tx_deinit();
    }
}

uint8_t fuzz_engine_queued_count(void)
{
    return s_queue.count;
}

void fuzz_engine_task(void)
{
    if (!s_running) return;

    /* Time budget enforcement */
    if (s_policy.time_budget_ms > 0) {
        uint32_t elapsed_ms = (time_us_32() - s_start_us) / 1000u;
        if (elapsed_ms >= s_policy.time_budget_ms) {
            fuzz_engine_stop();
            session_handle_stop();
            handle_get_status(g_session.session_id, 0);
            return;
        }
    }

    if (s_queue.count == 0) return;

    /* ── Select entry ───────────────────────────────────────────── */
    uint8_t pick;
    if (s_policy.selection_mode == 1u) {
        /* Random: pick any from the live range */
        pick = (uint8_t)((s_queue.head + (prng_next() % s_queue.count))
                         % FUZZ_MAX_ENTRIES);
    } else {
        /* Sequential (modes 0 and 2) */
        if (s_policy.repeat_mode == 1u) {
            pick = (uint8_t)((s_queue.head + s_queue.repeat_cursor)
                             % FUZZ_MAX_ENTRIES);
            s_queue.repeat_cursor = (uint8_t)(
                (s_queue.repeat_cursor + 1u) % s_queue.count);
        } else {
            pick = s_queue.head;
        }
    }

    fuzz_entry_t *entry = &s_queue.entries[pick];

    /* ── Working copy for mutation ──────────────────────────────── */
    static uint8_t work_buf[FUZZ_WORK_BUF];
    uint16_t work_len = entry->data_len;
    if (work_len > FUZZ_WORK_BUF) work_len = FUZZ_WORK_BUF;
    if (work_len > 0) {
        memcpy(work_buf, s_queue.pool + entry->pool_offset, work_len);
    }

    /* ── Data-level mutations ───────────────────────────────────── */
    if (s_policy.policy_flags & FUZZ_POLICY_BIT_FLIP) {
        mutate_bit_flip(work_buf, work_len);
    }
    if (s_policy.policy_flags & FUZZ_POLICY_TRUNCATE) {
        mutate_truncate(&work_len);
    }

    /* ── Dispatch to the active bus ─────────────────────────────── */
    if (g_session.active_bus == TARGET_BUS_UART) {
        /* Map policy flags → UART TX flags */
        uint8_t tx_flags = 0;
        if (s_policy.policy_flags & FUZZ_POLICY_CORRUPT_PARITY)
            tx_flags |= FUZZ_TX_FLAG_CORRUPT_PARITY;
        if (s_policy.policy_flags & FUZZ_POLICY_BAD_STOP_BIT)
            tx_flags |= FUZZ_TX_FLAG_BAD_STOP_BIT;
        if (s_policy.policy_flags & FUZZ_POLICY_TIMING_DISTORT)
            tx_flags |= FUZZ_TX_FLAG_TIMING_DISTORT;

        fuzz_uart_tx_send(work_buf, work_len, tx_flags);
        emit_fuzz_tx(entry->stimulus_id, work_buf, work_len, tx_flags);

    } else if (g_session.active_bus == TARGET_BUS_I2C) {
        /* Map policy flags → I2C TX flags */
        uint8_t i2c_flags = 0;
        if (s_policy.policy_flags & FUZZ_POLICY_I2C_SKIP_ACK)
            i2c_flags |= FUZZ_I2C_FLAG_SKIP_ACK;
        if (s_policy.policy_flags & FUZZ_POLICY_I2C_REPEATED_START)
            i2c_flags |= FUZZ_I2C_FLAG_REPEATED_START;
        if (s_policy.policy_flags & FUZZ_POLICY_I2C_CLOCK_STRETCH)
            i2c_flags |= FUZZ_I2C_FLAG_CLOCK_STRETCH;

        fuzz_i2c_tx_send(work_buf, work_len, i2c_flags);
        emit_fuzz_tx(entry->stimulus_id, work_buf, work_len, i2c_flags);
    }

    /* ── Stimulus lifecycle ─────────────────────────────────────── */
    if (s_policy.repeat_mode == 1u) {
        /* repeat: entries stay in queue, repeat_cursor already advanced */
    } else {
        /* once (0) or mutate-once (2): consume the entry at head */
        s_queue.head = (uint8_t)((s_queue.head + 1u) % FUZZ_MAX_ENTRIES);
        s_queue.count--;
        if (s_queue.count == 0u) {
            /* Reclaim pool and notify desktop */
            s_queue.pool_used     = 0;
            s_queue.head          = 0;
            s_queue.repeat_cursor = 0;
            
            /* Transition back to ARMED state for next session */
            fuzz_engine_stop();
            session_handle_stop();
            
            handle_get_status(g_session.session_id, 0);
        }
    }
}
