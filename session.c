#include "pico_host.h"

#include <poll.h>
#include <signal.h>
#include <errno.h>

static pico_result_t send_no_payload(pico_session_t *s, msg_type_t type)
{
    uint8_t buf[HW_PROTOCOL_HEADER_SIZE];
    size_t  n = frame_build(s, type, NULL, 0, buf, sizeof(buf));
    if (!n) return PICO_ERR_PROTOCOL;
    return transport_write(s, buf, n);
}

static pico_result_t send_payload(pico_session_t *s, msg_type_t type,
                                   const uint8_t *payload, uint16_t payload_len)
{
    uint8_t buf[TX_BUF_CAP];
    size_t  n = frame_build(s, type, payload, payload_len, buf, sizeof(buf));
    if (!n) return PICO_ERR_PROTOCOL;
    return transport_write(s, buf, n);
}

static pico_result_t require_state(pico_session_t *s,
                                    hw_protocol_session_state_t expected)
{
    if (s->state != expected) {
        fprintf(stderr, "[session] Zły stan: %s, wymagany %s\n",
                state_name(s->state), state_name(expected));
        return PICO_ERR_STATE;
    }
    return PICO_OK;
}

static pico_result_t require_state_any(pico_session_t *s,
                                       hw_protocol_session_state_t first,
                                       hw_protocol_session_state_t second)
{
    if (s->state == first || s->state == second) {
        return PICO_OK;
    }

    fprintf(stderr, "[session] Zły stan: %s, wymagany %s albo %s\n",
            state_name(s->state), state_name(first), state_name(second));
    return PICO_ERR_STATE;
}

// Komendy do Pico
pico_result_t session_hello(pico_session_t *s)
{
    // HELLO można wysłać z każdego stanu (reset połączenia)
    hw_protocol_hello_t h = {
        .requested_protocol = HW_PROTOCOL_VERSION_V1,
        .client_flags       = HW_PROTOCOL_CLIENT_WANTS_CAPTURE
                            | HW_PROTOCOL_CLIENT_WANTS_FUZZ,
        .client_version     = 0x0100,   // 1.0
        .reserved           = 0,
    };
    printf("[session] → HELLO\n");
    return send_payload(s, MSG_HELLO, (const uint8_t *)&h, sizeof(h));
}

pico_result_t session_get_caps(pico_session_t *s)
{
    // GET_CAPS nie ma payloadu
    printf("[session] → GET_CAPS\n");
    return send_no_payload(s, MSG_GET_CAPS);
}

pico_result_t session_get_status(pico_session_t *s)
{
    printf("[session] → GET_STATUS\n");
    return send_no_payload(s, MSG_GET_STATUS);
}

pico_result_t session_set_bus(pico_session_t *s, const hw_protocol_set_bus_t *cfg)
{
    pico_result_t r = require_state_any(s,
                                        HW_PROTOCOL_STATE_CAPABILITIES_READ,
                                        HW_PROTOCOL_STATE_CONFIGURED);
    if (r) return r;

    uint8_t buf[sizeof(hw_protocol_set_bus_t)];
    buf[0]  = (uint8_t)(cfg->speed_hz & 0xFF);
    buf[1]  = (uint8_t)((cfg->speed_hz >>  8) & 0xFF);
    buf[2]  = (uint8_t)((cfg->speed_hz >> 16) & 0xFF);
    buf[3]  = (uint8_t)((cfg->speed_hz >> 24) & 0xFF);
    buf[4]  = cfg->bus_type;
    buf[5]  = cfg->bus_flags;
    buf[6]  = cfg->pin_a;
    buf[7]  = cfg->pin_b;
    buf[8]  = cfg->uart_parity;
    buf[9]  = cfg->uart_stop_bits;
    buf[10] = 0; /* reserved  */
    buf[11] = 0; /* reserved2 */

    printf("[session] → SET_BUS (bus=%u, speed=%u Hz, pin_a=%u, pin_b=%u)\n",
           cfg->bus_type, cfg->speed_hz, cfg->pin_a, cfg->pin_b);
    r = send_payload(s, MSG_SET_BUS, buf, sizeof(buf));
    if (!r) {
        s->state = HW_PROTOCOL_STATE_CONFIGURED;
    }
    return r;
}

pico_result_t session_set_target(pico_session_t *s, const hw_protocol_set_target_t *cfg)
{
    pico_result_t r = require_state_any(s,
                                        HW_PROTOCOL_STATE_CAPABILITIES_READ,
                                        HW_PROTOCOL_STATE_CONFIGURED);
    if (r) return r;

    uint8_t buf[sizeof(hw_protocol_set_target_t)];
    buf[0] = (uint8_t)(cfg->vtarget_mv & 0xFF);
    buf[1] = (uint8_t)(cfg->vtarget_mv >> 8);
    buf[2] = cfg->pin_dir_mask;
    buf[3] = cfg->pullup_mode;
    buf[4] = cfg->pullup_mask;
    buf[5] = 0; // reserved
    buf[6] = 0; // reserved2 lo
    buf[7] = 0; // reserved2 hi

    printf("[session] → SET_TARGET (vtarget=%u mV, pullup=%u)\n",
           cfg->vtarget_mv, cfg->pullup_mode);
    r = send_payload(s, MSG_SET_TARGET, buf, sizeof(buf));
    if (!r) {
        s->state = HW_PROTOCOL_STATE_CONFIGURED;
    }
    return r;
}

pico_result_t session_set_fuzz_policy(pico_session_t *s,
                                       const hw_protocol_set_fuzz_policy_t *pol)
{
    // Dozwolone przed ARM lub gdy Armed+Stopped
    if (s->state != HW_PROTOCOL_STATE_CONFIGURED &&
        s->state != HW_PROTOCOL_STATE_ARMED) {
        fprintf(stderr, "[session] SET_FUZZ_POLICY: wrong state %s\n",
                state_name(s->state));
        return PICO_ERR_STATE;
    }

    if (pol->max_pending > HW_PROTOCOL_MAX_PENDING_STIMULI ||
        pol->pending_bytes > HW_PROTOCOL_MAX_PENDING_STIMULUS_BYTES) {
        fprintf(stderr, "[session] SET_FUZZ_POLICY: max_pending=%u or pending_bytes=%u "
                "is over firmware limits\n", pol->max_pending, pol->pending_bytes);
        return PICO_ERR_PROTOCOL;
    }

    uint8_t buf[sizeof(hw_protocol_set_fuzz_policy_t)];
    buf[0]  = (uint8_t)(pol->time_budget_ms & 0xFF);
    buf[1]  = (uint8_t)((pol->time_budget_ms >>  8) & 0xFF);
    buf[2]  = (uint8_t)((pol->time_budget_ms >> 16) & 0xFF);
    buf[3]  = (uint8_t)((pol->time_budget_ms >> 24) & 0xFF);
    buf[4]  = (uint8_t)(pol->pending_bytes & 0xFF);
    buf[5]  = (uint8_t)(pol->pending_bytes >> 8);
    buf[6]  = pol->policy_flags;
    buf[7]  = pol->selection_mode;
    buf[8]  = pol->repeat_mode;
    buf[9]  = pol->max_pending;
    buf[10] = 0;
    buf[11] = 0;

    printf("[session] → SET_FUZZ_POLICY (budget=%u ms, max_pending=%u)\n",
           pol->time_budget_ms, pol->max_pending);
    return send_payload(s, MSG_SET_FUZZ_POLICY, buf, sizeof(buf));
}

pico_result_t session_queue_stimulus(pico_session_t *s, uint32_t id,
                                      const uint8_t *data, uint16_t data_len,
                                      uint8_t flags, uint8_t kind)
{
    size_t total = 8u + data_len;
    if (total > TX_BUF_CAP) return PICO_ERR_PROTOCOL;

    uint8_t buf[TX_BUF_CAP];
    buf[0] = (uint8_t)(id & 0xFF);
    buf[1] = (uint8_t)((id >>  8) & 0xFF);
    buf[2] = (uint8_t)((id >> 16) & 0xFF);
    buf[3] = (uint8_t)((id >> 24) & 0xFF);
    buf[4] = (uint8_t)(data_len & 0xFF);
    buf[5] = (uint8_t)(data_len >> 8);
    buf[6] = flags;
    buf[7] = kind;
    if (data_len) memcpy(buf + 8, data, data_len);

    printf("[session] → QUEUE_STIMULUS id=%u len=%u\n", id, data_len);
    return send_payload(s, MSG_QUEUE_STIMULUS, buf, (uint16_t)total);
}

pico_result_t session_arm(pico_session_t *s)
{
    pico_result_t r = require_state(s, HW_PROTOCOL_STATE_CONFIGURED);
    if (r) return r;

    uint8_t buf[4];
    buf[0] = (uint8_t)(s->session_id & 0xFF);
    buf[1] = (uint8_t)(s->session_id >> 8);
    buf[2] = 0x00;  // arm_flags
    buf[3] = 0x00;  // reserved

    printf("[session] → ARM (session_id=0x%04X)\n", s->session_id);
    return send_payload(s, MSG_ARM, buf, sizeof(buf));
}

pico_result_t session_start_capture(pico_session_t *s)
{
    pico_result_t r = require_state(s, HW_PROTOCOL_STATE_ARMED);
    if (r) return r;
    printf("[session] → START_CAPTURE\n");
    r = send_no_payload(s, MSG_START_CAPTURE);
    if (r == PICO_OK) {
        s->state = HW_PROTOCOL_STATE_RUNNING;
    }
    return r;
}

pico_result_t session_start_fuzz(pico_session_t *s)
{
    pico_result_t r = require_state(s, HW_PROTOCOL_STATE_ARMED);
    if (r) return r;
    printf("[session] → START_FUZZ\n");
    r = send_no_payload(s, MSG_START_FUZZ);
    if (r == PICO_OK) {
        s->state = HW_PROTOCOL_STATE_RUNNING;
    }
    return r;
}

pico_result_t session_stop(pico_session_t *s)
{
    if (s->state != HW_PROTOCOL_STATE_RUNNING &&
        s->state != HW_PROTOCOL_STATE_ARMED) {
        fprintf(stderr, "[session] STOP: not Running or Armed (%s)\n",
                state_name(s->state));
        return PICO_ERR_STATE;
    }
    printf("[session] → STOP\n");
    return send_no_payload(s, MSG_STOP);
}

pico_result_t session_disarm(pico_session_t *s)
{
    printf("[session] → DISARM\n");
    return send_no_payload(s, MSG_DISARM);
}

pico_result_t session_reset(pico_session_t *s)
{
    printf("[session] → RESET_SESSION\n");
    s->session_id = 0;
    s->seq        = 0;
    return send_no_payload(s, MSG_RESET_SESSION);
}

// Obsługa ramek
static void handle_hello_ack(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 8) { fprintf(stderr, "[session] HELLO_ACK is too short\n"); return; }

    uint8_t neg_proto = payload[0];
    uint8_t fw_flags  = payload[1];
    uint16_t fw_ver   = (uint16_t)(payload[2] | ((uint16_t)payload[3] << 8));
    uint16_t sess_id  = (uint16_t)(payload[4] | ((uint16_t)payload[5] << 8));

    s->session_id = sess_id;
    s->state      = HW_PROTOCOL_STATE_CONNECTED;

    printf("[session] ← HELLO_ACK proto=%u fw_ver=0x%04X session=0x%04X flags=0x%02X\n",
           neg_proto, fw_ver, sess_id, fw_flags);
}

static void handle_caps_response(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 16) { fprintf(stderr, "[session] CAPS_RESPONSE is too short\n"); return; }

    uint32_t buf_bytes  = (uint32_t)(payload[0] | ((uint32_t)payload[1] << 8)
                         | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24));
    uint16_t max_burst  = (uint16_t)(payload[4] | ((uint16_t)payload[5] << 8));
    uint8_t  fw_ver     = payload[6];
    uint8_t  proto_ver  = payload[7];
    uint8_t  bus_mask   = payload[8];
    uint8_t  modes      = payload[9];
    uint8_t  sm_count   = payload[10];
    uint8_t  pin_count  = payload[12];

    printf("[session] ← CAPS_RESPONSE buf=%u B, burst=%u B, fw=%u, proto=%u, "
           "bus_mask=0x%02X, modes=0x%02X, PIO_SM=%u, pins=%u\n",
           buf_bytes, max_burst, fw_ver, proto_ver,
           bus_mask, modes, sm_count, pin_count);

    s->state = HW_PROTOCOL_STATE_CAPABILITIES_READ;
}

static void handle_arm_ok(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 4) { fprintf(stderr, "[session] ARM_OK is too short\n"); return; }
    uint16_t sess = (uint16_t)(payload[0] | ((uint16_t)payload[1] << 8));
    uint8_t  st   = payload[2];
    printf("[session] ← ARM_OK session=0x%04X state=%u\n", sess, st);
    s->state = HW_PROTOCOL_STATE_ARMED;
}

static void handle_stop_ok(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 8) { fprintf(stderr, "[session] STOP_OK is too short\n"); return; }
    uint32_t drained = (uint32_t)(payload[0] | ((uint32_t)payload[1] << 8)
                      | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24));
    uint16_t sess    = (uint16_t)(payload[4] | ((uint16_t)payload[5] << 8));
    printf("[session] ← STOP_OK drained=%u B, session=0x%04X\n", drained, sess);
    s->state = HW_PROTOCOL_STATE_ARMED;
}

static void handle_status(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 20) { fprintf(stderr, "[session] STATUS is too short\n"); return; }
    uint32_t rx_ov  = (uint32_t)(payload[0] | ((uint32_t)payload[1] << 8)
                     | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24));
    uint32_t tx_un  = (uint32_t)(payload[4] | ((uint32_t)payload[5] << 8)
                     | ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24));
    uint8_t  state  = payload[16];
    uint8_t  queued = payload[18];
    printf("[session] ← STATUS rx_overruns=%u tx_underruns=%u state=%s queued=%u\n",
           rx_ov, tx_un, state_name((hw_protocol_session_state_t)state), queued);
    s->state = (hw_protocol_session_state_t)state;
}

static void handle_trace_decoded(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 12) { fprintf(stderr, "[session] TRACE_DECODED is too short\n"); return; }

    hw_protocol_trace_decoded_t tr;
    tr.trace_seq    = (uint32_t)(payload[0] | ((uint32_t)payload[1] << 8)
                     | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24));
    tr.timestamp_us = (uint32_t)(payload[4] | ((uint32_t)payload[5] << 8)
                     | ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24));
    tr.data_len     = (uint16_t)(payload[8]  | ((uint16_t)payload[9] << 8));
    tr.source_bus   = payload[10];
    tr.event_type   = payload[11];

    static const char *ev[] = {"BYTE","START","STOP","ACK","NACK","BREAK","OVERFLOW"};
    const char *ev_name = (tr.event_type < 7) ? ev[tr.event_type] : "?";

    printf("[trace] seq=%-6u t=%8u us bus=%s event=%-8s len=%u",
           tr.trace_seq, tr.timestamp_us,
           tr.source_bus ? "UART" : "I2C ", ev_name, tr.data_len);

    if (tr.data_len && len >= 12u + tr.data_len) {
        printf("  data:");
        uint16_t show = tr.data_len < 8 ? tr.data_len : 8;
        for (uint16_t i = 0; i < show; i++)
            printf(" %02X", payload[12 + i]);
        if (tr.data_len > 8) printf(" ...");
    }
    printf("\n");

    hw_protocol_frame_header_t dummy_hdr = {0};
    csv_log_trace(s, &dummy_hdr, &tr);
}

static void handle_error(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 8) { fprintf(stderr, "[session] ERROR za krótki\n"); return; }
    uint16_t ctx_code = (uint16_t)(payload[0] | ((uint16_t)payload[1] << 8));
    uint16_t err_code = (uint16_t)(payload[2] | ((uint16_t)payload[3] << 8));
    uint16_t msg_len  = (uint16_t)(payload[4] | ((uint16_t)payload[5] << 8));
    uint8_t  sev      = payload[6];
    static const char *sev_names[] = {"INFO","WARN","ERROR","FATAL"};
    const char *sev_str = (sev < 4) ? sev_names[sev] : "?";

    fprintf(stderr, "[session] ← ERROR [%s] ctx=0x%04X code=0x%04X",
            sev_str, ctx_code, err_code);
    if (msg_len && len >= 8u + msg_len) {
        fprintf(stderr, " msg=\"%.*s\"", (int)msg_len, (const char *)(payload + 8));
    }
    fprintf(stderr, "\n");

    if (sev >= HW_PROTOCOL_SEVERITY_FATAL)
        s->state = HW_PROTOCOL_STATE_FAULT;
}

static void handle_fuzz_tx(pico_session_t *s, const uint8_t *payload, uint16_t len)
{
    if (len < 12) { fprintf(stderr, "[session] FUZZ_TX za krótki\n"); return; }
    uint32_t stim_id   = (uint32_t)(payload[0] | ((uint32_t)payload[1] << 8)
                        | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24));
    uint32_t trace_seq = (uint32_t)(payload[4] | ((uint32_t)payload[5] << 8)
                        | ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24));
    uint16_t data_len  = (uint16_t)(payload[10] | ((uint16_t)payload[11] << 8));
    printf("[fuzz]  ← FUZZ_TX stim_id=%u trace_seq=%u data_len=%u\n",
           stim_id, trace_seq, data_len);
    (void)s;
}

pico_result_t session_pump(pico_session_t *s)
{
    pico_result_t tr = transport_read(s);
    if (tr) return tr;

    size_t offset = 0;
    while (offset < s->rx_len) {
        hw_protocol_frame_header_t hdr;
        const uint8_t *payload;
        size_t consumed;

        parse_result_t pr = frame_parse(s->rx_buf + offset,
                                         s->rx_len - offset,
                                         &hdr, &payload, &consumed);
        if (pr == PARSE_NEED_MORE)
            break;

        offset += consumed;

        if (pr == PARSE_BAD_MAGIC) {
            continue;
        }
        if (pr == PARSE_BAD_CRC) {
            s->crc_errors++;
            fprintf(stderr, "[pump] CRC error (total: %llu)\n",
                    (unsigned long long)s->crc_errors);
            continue;
        }

        s->frames_rx++;

        msg_type_t type = (msg_type_t)hdr.type;
        switch (type) {
            case MSG_HELLO_ACK:    handle_hello_ack(s, payload, hdr.length);     break;
            case MSG_CAPS_RESPONSE:handle_caps_response(s, payload, hdr.length); break;
            case MSG_ARM_OK:       handle_arm_ok(s, payload, hdr.length);        break;
            case MSG_STOP_OK:      handle_stop_ok(s, payload, hdr.length);       break;
            case MSG_STATUS:       handle_status(s, payload, hdr.length);        break;
            case MSG_TRACE_DECODED:handle_trace_decoded(s, payload, hdr.length); break;
            case MSG_FUZZ_TX:      handle_fuzz_tx(s, payload, hdr.length);       break;
            case MSG_ERROR:        handle_error(s, payload, hdr.length);         break;
            case MSG_COUNTERS:
                printf("[session] ← COUNTERS (%u B)\n", hdr.length);
                break;
            default:
                printf("[session] ← unknown type 0x%02X (%u B)\n",
                       hdr.type, hdr.length);
                break;
        }

        if (s->on_frame) {
            s->on_frame(s->callback_user_data, &hdr, payload, hdr.length);
        }
    }

    if (offset > 0) {
        if (offset < s->rx_len)
            memmove(s->rx_buf, s->rx_buf + offset, s->rx_len - offset);
        s->rx_len -= offset;
    }

    return PICO_OK;
}
