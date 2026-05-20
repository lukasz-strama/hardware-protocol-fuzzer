#include "pico_host.h"

// CRC-16/CCITT-FALSE
// poly=0x1021, init=0xFFFF, xorout=0x0000, no reflect
uint16_t frame_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000u) ? (uint16_t)(crc << 1) ^ 0x1021u
                                  : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t frame_build(pico_session_t *s, msg_type_t type,
                   const uint8_t *payload, uint16_t payload_len,
                   uint8_t *out_buf, size_t out_cap)
{
    size_t total = HW_PROTOCOL_HEADER_SIZE + payload_len;
    if (out_cap < total) {
        fprintf(stderr, "[frame] out_buf too small (%zu < %zu)\n", out_cap, total);
        return 0;
    }

    memset(out_buf, 0, HW_PROTOCOL_HEADER_SIZE);

    out_buf[0]  = FRAME_MAGIC_0;           // magic[0]
    out_buf[1]  = FRAME_MAGIC_1;           // magic[1]
    out_buf[2]  = HW_PROTOCOL_VERSION_V1;  // version
    out_buf[3]  = (uint8_t)type;           // type
    out_buf[4]  = 0x00;                    // flags
    out_buf[5]  = 0x00;                    // padding
    out_buf[6]  = (uint8_t)(s->session_id & 0xFF);
    out_buf[7]  = (uint8_t)(s->session_id >> 8);
    uint32_t seq = s->seq++;
    out_buf[8]  = (uint8_t)(seq & 0xFF);
    out_buf[9]  = (uint8_t)((seq >>  8) & 0xFF);
    out_buf[10] = (uint8_t)((seq >> 16) & 0xFF);
    out_buf[11] = (uint8_t)((seq >> 24) & 0xFF);
    out_buf[12] = (uint8_t)(payload_len & 0xFF);
    out_buf[13] = (uint8_t)(payload_len >> 8);

    if (payload && payload_len)
        memcpy(out_buf + HW_PROTOCOL_HEADER_SIZE, payload, payload_len);

    uint16_t crc = frame_crc16(out_buf, 14);
    if (payload_len)
        crc = frame_crc16(out_buf + HW_PROTOCOL_HEADER_SIZE, payload_len);

    {
        uint16_t crc2 = 0xFFFF;
        for (int i = 0; i < 14; i++) {
            crc2 ^= (uint16_t)out_buf[i] << 8;
            for (int b = 0; b < 8; b++)
                crc2 = (crc2 & 0x8000u) ? (uint16_t)(crc2 << 1) ^ 0x1021u
                                        : (uint16_t)(crc2 << 1);
        }
        for (uint16_t i = 0; i < payload_len; i++) {
            crc2 ^= (uint16_t)out_buf[HW_PROTOCOL_HEADER_SIZE + i] << 8;
            for (int b = 0; b < 8; b++)
                crc2 = (crc2 & 0x8000u) ? (uint16_t)(crc2 << 1) ^ 0x1021u
                                        : (uint16_t)(crc2 << 1);
        }
        crc = crc2;
    }

    out_buf[14] = (uint8_t)(crc & 0xFF);
    out_buf[15] = (uint8_t)(crc >> 8);

    s->frames_tx++;
    return total;
}

parse_result_t frame_parse(const uint8_t *buf, size_t len,
                            hw_protocol_frame_header_t *hdr,
                            const uint8_t **payload_out,
                            size_t *consumed)
{
    *consumed    = 0;
    *payload_out = NULL;

    if (len < 2)
        return PARSE_NEED_MORE;

    if (buf[0] != FRAME_MAGIC_0 || buf[1] != FRAME_MAGIC_1) {
        *consumed = 1;
        return PARSE_BAD_MAGIC;
    }

    if (len < HW_PROTOCOL_HEADER_SIZE)
        return PARSE_NEED_MORE;

    uint16_t payload_len = (uint16_t)(buf[12] | ((uint16_t)buf[13] << 8));
    size_t   total       = HW_PROTOCOL_HEADER_SIZE + payload_len;

    if (len < total)
        return PARSE_NEED_MORE;

    uint16_t recv_crc = (uint16_t)(buf[14] | ((uint16_t)buf[15] << 8));
    uint16_t calc_crc = 0xFFFF;
    for (int i = 0; i < 14; i++) {
        calc_crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++)
            calc_crc = (calc_crc & 0x8000u) ? (uint16_t)(calc_crc << 1) ^ 0x1021u
                                            : (uint16_t)(calc_crc << 1);
    }
    for (uint16_t i = 0; i < payload_len; i++) {
        calc_crc ^= (uint16_t)buf[HW_PROTOCOL_HEADER_SIZE + i] << 8;
        for (int b = 0; b < 8; b++)
            calc_crc = (calc_crc & 0x8000u) ? (uint16_t)(calc_crc << 1) ^ 0x1021u
                                            : (uint16_t)(calc_crc << 1);
    }

    if (calc_crc != recv_crc) {
        *consumed = total;
        return PARSE_BAD_CRC;
    }

    hdr->magic[0]   = buf[0];
    hdr->magic[1]   = buf[1];
    hdr->version    = buf[2];
    hdr->type       = buf[3];
    hdr->flags      = buf[4];
    hdr->padding    = buf[5];
    hdr->session_id = (uint16_t)(buf[6]  | ((uint16_t)buf[7]  << 8));
    hdr->sequence   = (uint32_t)(buf[8]  | ((uint32_t)buf[9]  << 8)
                               | ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24));
    hdr->length     = payload_len;
    hdr->checksum   = recv_crc;

    *payload_out = buf + HW_PROTOCOL_HEADER_SIZE;
    *consumed    = total;
    return PARSE_OK;
}
