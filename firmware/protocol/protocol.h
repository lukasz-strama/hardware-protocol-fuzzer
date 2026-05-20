#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "protocol_layout.h"

//TYPY MESSAGES
#define MSG_TYPE_HELLO              0x01
#define MSG_TYPE_HELLO_ACK          0x02

#define MSG_TYPE_GET_CAPS           0x10
#define MSG_TYPE_CAPS_RESPONSE      0x11

#define MSG_TYPE_GET_STATUS         0x12
#define MSG_TYPE_STATUS             0x13

#define MSG_TYPE_SET_BUS            0x20
#define MSG_TYPE_SET_TARGET         0x21

#define MSG_TYPE_SET_FUZZ_POLICY    0x22
#define MSG_TYPE_QUEUE_STIMULUS     0x23

#define MSG_TYPE_ARM                0x30
#define MSG_TYPE_ARM_OK             0x31

#define MSG_TYPE_START_CAPTURE      0x40
#define MSG_TYPE_TRACE_DECODED      0x41

#define MSG_TYPE_START_FUZZ         0x42
#define MSG_TYPE_FUZZ_TX            0x43

#define MSG_TYPE_STOP               0x50
#define MSG_TYPE_STOP_OK            0x51

#define MSG_TYPE_DISARM             0x60
#define MSG_TYPE_RESET_SESSION      0x61

#define MSG_TYPE_ERROR              0x70
#define MSG_TYPE_COUNTERS           0x71


void protocol_send_frame(
    uint8_t type, 
    uint16_t session_id, 
    uint32_t sequence, 
    const uint8_t* payload, 
    size_t payload_len);

    typedef enum {
        STATE_WAIT_MAGIC1, 
        STATE_WAIT_MAGIC2,
        STATE_READ_HEADER,
        STATE_READ_PAYLOAD
    }rx_state_t;

    typedef struct {
    rx_state_t state;
    uint8_t header_buffer[sizeof(hw_protocol_frame_header_t)];
    size_t header_bytes_read;
    uint8_t payload_buffer[HW_PROTOCOL_MAX_TRACE_CHUNK]; 
    size_t payload_bytes_read;
} protocol_parser_t;


void protocol_parser_init(protocol_parser_t *parser);

bool protocol_parse_byte(protocol_parser_t *parser, uint8_t byte, hw_protocol_frame_header_t *out_header, uint8_t *out_payload);

#endif