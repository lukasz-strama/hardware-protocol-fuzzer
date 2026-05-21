#include "fuzz_engine.h"
#include "fuzz_uart_tx.h"
#include "protocol.h"
#include "protocol_handlers.h"
#include "protocol_layout.h"
#include "session.h"
#include "pico/time.h"
#include <string.h>

/* ------------------------------------------------------------------ limits */

#define FUZZ_MAX_ENTRIES  HW_PROTOCOL_MAX_PENDING_STIMULI        /* 32 */
#define FUZZ_POOL_SIZE    HW_PROTOCOL_MAX_PENDING_STIMULUS_BYTES  /* 4096 */
#define FUZZ_WORK_BUF     256u  /* max single-stimulus working copy */

/* ------------------------------------------------------------------ types */

typedef struct {
    uint32_t stimulus_id;
    uint16_t pool_offset;
    uint16_t data_len;
    uint8_t  stimulus_flags;
    uint8_t  stimulus_kind;
} fuzz_entry_t;

typedef struct {
    fuzz_entry_t entries[FUZZ_MAX_ENTRIES];
    uint8_t      pool[FUZZ_POOL_SIZE];
    uint16_t     pool_used;
    uint8_t      head;
    uint8_t      count;
    uint8_t      repeat_cursor;  /* cycles through entries in repeat mode */
} fuzz_queue_t;

/* ------------------------------------------------------------------ state */

static hw_protocol_set_fuzz_policy_t s_policy;
static fuzz_queue_t  s_queue;
static bool          s_running;
static uint32_t      s_start_us;
static uint32_t      s_prng;
static uint32_t      s_tx_seq;

/* ------------------------------------------------------------------ prng */

static uint32_t prng_next(void) {
    s_prng ^= s_prng << 13;
    s_prng ^= s_prng >> 17;
    s_prng ^= s_prng << 5;
    return s_prng;
}

/* ------------------------------------------------------------------ mutations */

static void mutate_bit_flip(uint8_t *buf, uint16_t len) {
    if (len == 0) return;
    uint32_t r       = prng_next();
    uint16_t byte_i  = (uint16_t)(r % len);
    uint8_t  bit_i   = (uint8_t)((r >> 8) & 7u);
    buf[byte_i] ^= (uint8_t)(1u << bit_i);
}

static void mutate_truncate(uint16_t *len) {
    if (*len <= 1) return;
    uint16_t new_len = (uint16_t)(1u + (prng_next() % (*len - 1u)));
    *len = new_len;
}

/* ------------------------------------------------------------------ FUZZ_TX report */

static void emit_fuzz_tx(uint32_t stimulus_id,
                         const uint8_t *data, uint16_t data_len,
                         uint8_t tx_flags)
{
    /* Build FUZZ_TX payload in a local buffer: fixed 12 B header + data */
    static uint8_t tx_buf[sizeof(hw_protocol_fuzz_tx_t) + FUZZ_WORK_BUF];

    uint16_t payload_len = (uint16_t)(sizeof(hw_protocol_fuzz_tx_t) + data_len);
    if (data_len > FUZZ_WORK_BUF) data_len = FUZZ_WORK_BUF;

    hw_protocol_fuzz_tx_t *hdr = (hw_protocol_fuzz_tx_t *)tx_buf;
    hdr->stimulus_id = stimulus_id;
    hdr->trace_seq   = s_tx_seq++;
    hdr->mode        = s_policy.selection_mode;
    hdr->flags       = tx_flags;
    hdr->data_len    = data_len;
    if (data_len > 0 && data != NULL) {
        memcpy(hdr->data, data, data_len);
    }

    protocol_send_frame(MSG_TYPE_FUZZ_TX,
                        g_session.session_id,
                        s_tx_seq,
                        tx_buf,
                        payload_len);
}

/* ------------------------------------------------------------------ public API */

void fuzz_engine_init(void) {
    memset(&s_policy, 0, sizeof(s_policy));
    memset(&s_queue, 0, sizeof(s_queue));
    s_running = false;
    s_start_us = 0;
    s_prng = 0xFEEDBEEFu;
    s_tx_seq = 0;
}

bool fuzz_engine_set_policy(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(hw_protocol_set_fuzz_policy_t)) return false;

    /* Deserialise manually — no packed cast per contract rules */
    uint32_t time_budget_ms = (uint32_t)payload[0]
                            | ((uint32_t)payload[1] << 8)
                            | ((uint32_t)payload[2] << 16)
                            | ((uint32_t)payload[3] << 24);
    uint16_t pending_bytes  = (uint16_t)payload[4] | ((uint16_t)payload[5] << 8);
    uint8_t  policy_flags   = payload[6];
    uint8_t  selection_mode = payload[7];
    uint8_t  repeat_mode    = payload[8];
    uint8_t  max_pending    = payload[9];

    if (max_pending > FUZZ_MAX_ENTRIES || pending_bytes > FUZZ_POOL_SIZE) {
        return false;
    }

    s_policy.time_budget_ms = time_budget_ms;
    s_policy.pending_bytes  = pending_bytes;
    s_policy.policy_flags   = policy_flags;
    s_policy.selection_mode = selection_mode;
    s_policy.repeat_mode    = repeat_mode;
    s_policy.max_pending    = max_pending ? max_pending : (uint8_t)FUZZ_MAX_ENTRIES;
    return true;
}

bool fuzz_engine_queue_stimulus(const uint8_t *payload, uint16_t total_len) {
    if (total_len < 8u) return false;  /* fixed header is 8 bytes */

    /* Deserialise QUEUE_STIMULUS fixed header */
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

    /* Queue capacity */
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

void fuzz_engine_start(void) {
    s_running  = true;
    s_start_us = time_us_32();
    s_queue.repeat_cursor = 0;

    fuzz_uart_tx_init(g_session.uart_tx_pin, g_session.uart_baudrate);
}

void fuzz_engine_stop(void) {
    if (!s_running) return;
    s_running = false;
    fuzz_uart_tx_deinit();
}

uint8_t fuzz_engine_queued_count(void) {
    return s_queue.count;
}

void fuzz_engine_task(void) {
    if (!s_running) return;

    /* Time budget check */
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

    /* Select entry */
    uint8_t pick;
    if (s_policy.selection_mode == 1u) {
        /* random: pick any from the live range */
        pick = (uint8_t)((s_queue.head + (prng_next() % s_queue.count)) % FUZZ_MAX_ENTRIES);
    } else {
        /* sequential (modes 0 and 2) */
        if (s_policy.repeat_mode == 1u) {
            pick = (uint8_t)((s_queue.head + s_queue.repeat_cursor) % FUZZ_MAX_ENTRIES);
            s_queue.repeat_cursor = (uint8_t)((s_queue.repeat_cursor + 1u) % s_queue.count);
        } else {
            pick = s_queue.head;
        }
    }

    fuzz_entry_t *entry = &s_queue.entries[pick];

    /* Working copy for mutation — original pool data is never modified */
    static uint8_t work_buf[FUZZ_WORK_BUF];
    uint16_t work_len = entry->data_len;
    if (work_len > FUZZ_WORK_BUF) work_len = FUZZ_WORK_BUF;
    if (work_len > 0) {
        memcpy(work_buf, s_queue.pool + entry->pool_offset, work_len);
    }

    /* Data-level mutations */
    if (s_policy.policy_flags & FUZZ_POLICY_BIT_FLIP) {
        mutate_bit_flip(work_buf, work_len);
    }
    if (s_policy.policy_flags & FUZZ_POLICY_TRUNCATE) {
        mutate_truncate(&work_len);
    }

    /* Physical-layer error injection flags */
    uint8_t tx_flags = 0;
    if (s_policy.policy_flags & FUZZ_POLICY_CORRUPT_PARITY) tx_flags |= FUZZ_TX_FLAG_CORRUPT_PARITY;
    if (s_policy.policy_flags & FUZZ_POLICY_BAD_STOP_BIT)   tx_flags |= FUZZ_TX_FLAG_BAD_STOP_BIT;
    if (s_policy.policy_flags & FUZZ_POLICY_TIMING_DISTORT)  tx_flags |= FUZZ_TX_FLAG_TIMING_DISTORT;

    /* Transmit */
    fuzz_uart_tx_send(work_buf, work_len, tx_flags);

    /* Report what was sent */
    emit_fuzz_tx(entry->stimulus_id, work_buf, work_len, tx_flags);

    /* Stimulus lifecycle based on repeat_mode */
    if (s_policy.repeat_mode == 1u) {
        /* repeat: entries stay in queue, repeat_cursor already advanced above */
    } else {
        /* once (0) or mutate-once (2): consume the entry at head */
        s_queue.head = (uint8_t)((s_queue.head + 1u) % FUZZ_MAX_ENTRIES);
        s_queue.count--;
        if (s_queue.count == 0u) {
            /* Reclaim entire pool and notify desktop the queue is drained */
            s_queue.pool_used = 0;
            s_queue.head = 0;
            s_queue.repeat_cursor = 0;
            handle_get_status(g_session.session_id, 0);
        }
    }
}
