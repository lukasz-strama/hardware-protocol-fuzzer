#include "protocol_handlers.h"
#include "protocol.h"
#include "protocol_layout.h"
#include "session.h"
#include <string.h>

#ifndef HW_PROTOCOL_FW_SUPPORTS_CAPTURE
#define HW_PROTOCOL_FW_SUPPORTS_CAPTURE (HW_PROTOCOL_FW_SUPPORTS_STREAMING)
#endif

void handle_get_caps(uint16_t session_id, uint32_t seq) {
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    hw_protocol_caps_response_t *caps = (hw_protocol_caps_response_t *)buf;

    caps->buffer_bytes     = 4096;
    caps->max_burst_bytes  = 1024;
    caps->fw_version       = 1;
    caps->protocol_version = HW_PROTOCOL_VERSION_V1;

    caps->bus_mask = (1u << HW_PROTOCOL_BUS_UART);

    caps->supported_modes  = HW_PROTOCOL_MODE_CAPTURE;

    caps->pio_sm_count     = 2;
    caps->reserved         = 0;
    caps->pin_map_count    = 0;

    protocol_send_frame(MSG_TYPE_CAPS_RESPONSE,
                        session_id,
                        seq,
                        buf,
                        sizeof(hw_protocol_caps_response_t));
}

void handle_get_status(uint16_t session_id, uint32_t seq) {
    hw_protocol_status_t st = {
        .rx_overruns     = 0,
        .tx_underruns    = 0,
        .armed_since_ms  = 0,
        .session_id      = session_id,
        .last_error      = 0,
        .state           = g_session.current_state,
        .flags           = 0,
        .queued_stimuli  = 0,
        .reserved        = 0
    };

    protocol_send_frame(MSG_TYPE_STATUS,
                        session_id,
                        seq,
                        (const uint8_t *)&st,
                        sizeof(st));
}

void handle_hello(uint16_t session_id, uint32_t seq,
                  const uint8_t *payload, uint16_t len)
{
    (void)payload;
    (void)len;

    hw_protocol_hello_ack_t ack = {
        .negotiated_protocol = HW_PROTOCOL_VERSION_V1,
        .fw_flags            = HW_PROTOCOL_FW_SUPPORTS_STREAMING |
                               HW_PROTOCOL_FW_REQUIRES_EXTERNAL_PULLUPS,
        .fw_version          = 1,
        .session_id          = session_id,
        .reserved            = 0
    };

    g_session.current_state = HW_PROTOCOL_STATE_CONNECTED;

    protocol_send_frame(MSG_TYPE_HELLO_ACK,
                        session_id,
                        seq,
                        (const uint8_t *)&ack,
                        sizeof(ack));
}

void handle_arm(uint16_t session_id, uint32_t seq,
                const uint8_t *payload, uint16_t len)
{
    (void)payload;
    (void)len;

    session_handle_arm(session_id);

    hw_protocol_arm_ok_t ok = {
        .session_id = session_id,
        .state      = g_session.current_state,
        .reserved   = 0
    };

    protocol_send_frame(MSG_TYPE_ARM_OK,
                        session_id,
                        seq,
                        (const uint8_t *)&ok,
                        sizeof(ok));
}

void handle_start_capture(uint16_t session_id, uint32_t seq) {
    session_handle_start_capture();
    handle_get_status(session_id, seq);
}

void handle_stop(uint16_t session_id, uint32_t seq)
{
    uint32_t drained = session_handle_stop();

    hw_protocol_stop_ok_t ok = {
        .drained_bytes = drained,
        .session_id    = session_id,
        .state         = g_session.current_state,
        .reserved      = 0
    };

    protocol_send_frame(MSG_TYPE_STOP_OK,
                        session_id,
                        seq,
                        (const uint8_t *)&ok,
                        sizeof(ok));
}

void handle_disarm(uint16_t session_id, uint32_t seq)
{
    session_handle_disarm();
    handle_get_status(session_id, seq);
}

void handle_reset_session(uint16_t session_id, uint32_t seq)
{
    (void)session_id;
    (void)seq;
    session_init();
}

void handle_unknown(uint16_t session_id, uint32_t seq, uint8_t type)
{
    hw_protocol_error_t err = {
        .context_code = 0,
        .error_code   = type,
        .message_len  = 0,
        .severity     = HW_PROTOCOL_SEVERITY_ERROR,
        .reserved     = 0
    };

    protocol_send_frame(MSG_TYPE_ERROR,
                        session_id,
                        seq,
                        (const uint8_t *)&err,
                        sizeof(err));
}
