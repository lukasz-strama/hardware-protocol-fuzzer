/**
 * @file protocol.c
 * @brief Implementacja warstwy transportowej protokołu oraz parsera ramek.
 *
 * Moduł odpowiada za:
 * - budowanie ramek protokołu (nagłówek + payload),
 * - obliczanie CRC16-CCITT-FALSE,
 * - wysyłanie ramek przez warstwę USB,
 * - parser bajt po bajcie z maszyną stanów,
 * - walidację CRC i długości payloadu.
 *
 * Parser jest odporny na zakłócenia - w przypadku błędu wraca do stanu
 * oczekiwania na MAGIC1.
 */
#include "protocol.h"
#include "protocol_layout.h"
#include <string.h>
#include "usb_transport.h"

/**
 * @brief Implementacja CRC16-CCITT-FALSE.
 *
 * Parametry:
 * - polinom: 0x1021
 * - seed: 0xFFFF
 * - brak odbicia bitów
 *
 * @param crc Wartość początkowa CRC.
 * @param data Dane wejściowe.
 * @param len Liczba bajtów.
 * @return Zaktualizowana wartość CRC.
 */
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

/**
 * @brief Buduje i wysyła ramkę protokołu.
 *
 * Funkcja:
 * - wypełnia nagłówek,
 * - oblicza CRC16 nagłówka i payloadu,
 * - kopiuje dane do bufora ramki,
 * - wysyła ramkę przez USB.
 *
 * @param type Typ wiadomości (MSG_TYPE_xxx).
 * @param session_id Identyfikator sesji.
 * @param sequence Numer sekwencyjny ramki.
 * @param payload Dane payloadu (może być NULL).
 * @param payload_len Długość payloadu.
 */
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

/**
 * @brief Inicjalizuje parser protokołu.
 *
 * Ustawia parser w stan oczekiwania na pierwszy bajt MAGIC.
 *
 * @param parser Wskaźnik na parser.
 */
void protocol_parser_init(protocol_parser_t *parser) {
    parser->state = STATE_WAIT_MAGIC1;
    parser->header_bytes_read = 0;
    parser->payload_bytes_read = 0;
}

/**
 * @brief Przetwarza pojedynczy bajt strumienia danych.
 *
 * Parser implementuje maszynę stanów:
 * - WAIT_MAGIC1
 * - WAIT_MAGIC2
 * - READ_HEADER
 * - READ_PAYLOAD
 *
 * Po zdekodowaniu kompletnej ramki:
 * - kopiuje nagłówek do out_header,
 * - kopiuje payload do out_payload,
 * - weryfikuje CRC,
 * - zwraca true.
 *
 * W przypadku błędu wraca do stanu WAIT_MAGIC1.
 *
 * @param parser Parser protokołu.
 * @param byte Bajt wejściowy.
 * @param out_header Bufor na nagłówek.
 * @param out_payload Bufor na payload.
 * @return true jeśli ramka została poprawnie zdekodowana.
 */
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

/**
 * @brief Main protocol task.
 *
 * Called periodically from main loop. Currently just ensures USB transport
 * is serviced. Can be extended for protocol-level processing in future.
 *
 * @note This is called from Core 0 main loop, not from ISR context.
 */
void protocol_task(void) {
    /* USB transport handles all protocol parsing via ISR */
    /* This stub can be extended if additional protocol-level */
    /* processing is needed in the future */
}