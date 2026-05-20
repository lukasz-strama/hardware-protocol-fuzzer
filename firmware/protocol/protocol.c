#include "protocol.h"
#include "protocol_layout.h"
#include <string.h>
#include "usb_transport.h"


static uint16_t local_crc16_ccitt_false(uint16_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void protocol_send_frame(
    uint8_t type,
    uint16_t session_id,
    uint32_t sequence,
    const uint8_t* payload,
    size_t payload_len)
{
    hw_protocol_frame_header_t header;

    header.magic[0] = 0x55;
    header.magic[1] = 0xAA;

    header.version = HW_PROTOCOL_VERSION_V1;
    header.type = type;
    header.flags = 0;
    header.padding = 0;
    header.session_id = session_id;
    header.sequence = sequence;
    header.length = payload_len;
    
    uint16_t crc = 0xFFFF;
    crc = local_crc16_ccitt_false(crc, (const uint8_t*)&header, 14);

    if (payload_len > 0 && payload != NULL) {
        crc = local_crc16_ccitt_false(crc, payload, payload_len);
    }

    header.checksum = crc;

    static uint8_t frame_buf[HW_PROTOCOL_HEADER_SIZE + HW_PROTOCOL_MAX_TRACE_CHUNK];
    memcpy(frame_buf, &header, sizeof(hw_protocol_frame_header_t));
    if (payload_len > 0 && payload != NULL) {
        memcpy(frame_buf + sizeof(hw_protocol_frame_header_t), payload, payload_len);
    }
    usb_transport_send(frame_buf, sizeof(hw_protocol_frame_header_t) + payload_len);
}

void protocol_parser_init(protocol_parser_t *parser) {
    parser->state = STATE_WAIT_MAGIC1;
    parser->header_bytes_read = 0;
    parser->payload_bytes_read = 0;
}

bool protocol_parse_byte(
    protocol_parser_t *parser,
    uint8_t byte,
    hw_protocol_frame_header_t *out_header,
    uint8_t *out_payload)
{
    switch (parser->state) {

    case STATE_WAIT_MAGIC1:
        if (byte == 0x55) {
            parser->header_buffer[0] = byte;
            parser->state = STATE_WAIT_MAGIC2;
        }
        break;

    case STATE_WAIT_MAGIC2:
        if (byte == 0xAA) {
            parser->header_buffer[1] = byte;
            parser->header_bytes_read = 2;
            parser->state = STATE_READ_HEADER;
        } else {
            parser->state = STATE_WAIT_MAGIC1;
        }
        break;

    case STATE_READ_HEADER:
        parser->header_buffer[parser->header_bytes_read++] = byte;

        if (parser->header_bytes_read == sizeof(hw_protocol_frame_header_t)) {
            memcpy(out_header, parser->header_buffer, sizeof(hw_protocol_frame_header_t));

            if (out_header->length == 0) {
                uint16_t calc = local_crc16_ccitt_false(0xFFFF, parser->header_buffer, 14);
                parser->state = STATE_WAIT_MAGIC1;
                return (calc == out_header->checksum);
            }

            parser->payload_bytes_read = 0;
            parser->state = STATE_READ_PAYLOAD;
        }
        break;

    case STATE_READ_PAYLOAD:
        parser->payload_buffer[parser->payload_bytes_read++] = byte;

        if (parser->payload_bytes_read == out_header->length) {
            uint16_t calc = local_crc16_ccitt_false(0xFFFF, parser->header_buffer, 14);
            calc = local_crc16_ccitt_false(calc, parser->payload_buffer, out_header->length);

            memcpy(out_payload, parser->payload_buffer, out_header->length);

            parser->state = STATE_WAIT_MAGIC1;
            return (calc == out_header->checksum);
        }
        break;
    }

    return false;
}