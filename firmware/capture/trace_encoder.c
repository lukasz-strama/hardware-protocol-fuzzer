#include "trace_encoder.h"
#include "protocol.h"
#include "protocol_layout.h"
#include "session.h"
#include <stdint.h>
#include <string.h>

static uint32_t trace_seq = 0;

static uint8_t trace_buf[sizeof(hw_protocol_trace_decoded_t) + 256];

void trace_emit(uint32_t timestamp_us,
                uint8_t source_bus,
                uint8_t event_type,
                const uint8_t *data,
                uint16_t len)
{
    if (len > 256) {
        len = 256; // przed przepelnieniem
    }

    hw_protocol_trace_decoded_t *t = (hw_protocol_trace_decoded_t *)trace_buf;

    t->trace_seq    = trace_seq++;
    t->timestamp_us = timestamp_us;
    t->data_len     = len;
    t->source_bus   = source_bus;
    t->event_type   = event_type;

    if (len > 0 && data != NULL) {
        memcpy(t->data, data, len);
    }

    protocol_send_frame(
        MSG_TYPE_TRACE_DECODED,
        g_session.session_id,
        trace_seq,
        trace_buf,
        sizeof(hw_protocol_trace_decoded_t) + len
    );
}