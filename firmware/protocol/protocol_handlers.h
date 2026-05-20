#ifndef PROTOCOL_HANDLERS_H
#define PROTOCOL_HANDLERS_H

#include <stdint.h>

void handle_get_caps(uint16_t session_id, uint32_t seq);
void handle_get_status(uint16_t session_id, uint32_t seq);
void handle_hello(uint16_t session_id, uint32_t seq, const uint8_t *payload, uint16_t len);
void handle_arm(uint16_t session_id, uint32_t seq, const uint8_t *payload, uint16_t len);
void handle_start_capture(uint16_t session_id, uint32_t seq);
void handle_set_fuzz_policy(uint16_t session_id, uint32_t seq, const uint8_t *payload, uint16_t len);
void handle_queue_stimulus(uint16_t session_id, uint32_t seq, const uint8_t *payload, uint16_t len);
void handle_start_fuzz(uint16_t session_id, uint32_t seq);
void handle_stop(uint16_t session_id, uint32_t seq);
void handle_disarm(uint16_t session_id, uint32_t seq);
void handle_reset_session(uint16_t session_id, uint32_t seq);
void handle_unknown(uint16_t session_id, uint32_t seq, uint8_t type);


#endif
