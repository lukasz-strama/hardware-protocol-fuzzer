#include "protocol_handlers_dispatcher.h"
#include "protocol.h"
#include "protocol_handlers.h"
#include "session.h"
#include <stdint.h>

void protocol_handle_frame(const hw_protocol_frame_header_t *hdr,
                           const uint8_t *payload)
{
    switch (hdr->type) {
    case MSG_TYPE_HELLO:
        handle_hello(hdr->session_id, hdr->sequence, payload, hdr->length);
        break;
    case MSG_TYPE_GET_CAPS:
        handle_get_caps(hdr->session_id, hdr->sequence);
        break;
    case MSG_TYPE_GET_STATUS:
        handle_get_status(hdr->session_id, hdr->sequence);
        break;
    case MSG_TYPE_SET_BUS:
        session_handle_set_bus(payload);
        break;
    case MSG_TYPE_SET_TARGET:
        session_handle_set_target(payload);
        break;
    case MSG_TYPE_ARM:
        handle_arm(hdr->session_id, hdr->sequence, payload, hdr->length);
        break;
    case MSG_TYPE_START_CAPTURE:
        handle_start_capture(hdr->session_id, hdr->sequence);
        break;
    case MSG_TYPE_SET_FUZZ_POLICY:
        handle_set_fuzz_policy(hdr->session_id, hdr->sequence, payload, hdr->length);
        break;
    case MSG_TYPE_QUEUE_STIMULUS:
        handle_queue_stimulus(hdr->session_id, hdr->sequence, payload, hdr->length);
        break;
    case MSG_TYPE_START_FUZZ:
        handle_start_fuzz(hdr->session_id, hdr->sequence);
        break;
    case MSG_TYPE_STOP:
        handle_stop(hdr->session_id, hdr->sequence);
        break;
    case MSG_TYPE_DISARM:
        handle_disarm(hdr->session_id, hdr->sequence);
        break;
    case MSG_TYPE_RESET_SESSION:
        handle_reset_session(hdr->session_id, hdr->sequence);
        break;
    default:
        handle_unknown(hdr->session_id, hdr->sequence, hdr->type);
        break;
    }
}
