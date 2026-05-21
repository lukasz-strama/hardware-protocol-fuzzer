#ifndef PICO_HOST_H
#define PICO_HOST_H

#define _POSIX_C_SOURCE 200809L
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "protocol_layout.h"

//Kody typów komend (z contract.md)
typedef enum {
    // PC → Pico
    MSG_HELLO            = 0x01,
    MSG_GET_CAPS         = 0x10,
    MSG_GET_STATUS       = 0x12,
    MSG_SET_BUS          = 0x20,
    MSG_SET_TARGET       = 0x21,
    MSG_SET_FUZZ_POLICY  = 0x22,
    MSG_QUEUE_STIMULUS   = 0x23,
    MSG_ARM              = 0x30,
    MSG_START_CAPTURE    = 0x40,
    MSG_START_FUZZ       = 0x42,
    MSG_STOP             = 0x50,
    MSG_DISARM           = 0x60,
    MSG_RESET_SESSION    = 0x61,

    // Pico → PC
    MSG_CAPS_RESPONSE    = 0x11,
    MSG_HELLO_ACK        = 0x02,
    MSG_ARM_OK           = 0x31,
    MSG_STOP_OK          = 0x51,
    MSG_STATUS           = 0x13,
    MSG_TRACE_DECODED    = 0x41,
    MSG_FUZZ_TX          = 0x43,
    MSG_COUNTERS         = 0x71,
    MSG_ERROR            = 0x70,
} msg_type_t;

#define FRAME_MAGIC_0  0x55
#define FRAME_MAGIC_1  0xAA

// Bufor ramki roboczej
#define TX_BUF_CAP  (HW_PROTOCOL_HEADER_SIZE + HW_PROTOCOL_MAX_PENDING_STIMULUS_BYTES + 64u)
#define RX_BUF_CAP  (64u * 1024u)   // 64 KB ring dla danych z Pico

// Kontekst sesji
typedef struct {
    // transport
    int      fd;                    // deskryptor /dev/ttyUSBx lub /dev/ttyACMx
    char     port[64];

    // protokół
    hw_protocol_session_state_t state;
    uint16_t session_id;
    uint32_t seq;                   // numer sekwencji PC→Pico, inkrementowany

    // bufor odbiorczy
    uint8_t  rx_buf[RX_BUF_CAP];
    size_t   rx_len;

    // CSV
    FILE    *csv_fp;
    char     csv_path[256];
    uint64_t csv_rows;

    // statystyki
    uint64_t frames_tx;
    uint64_t frames_rx;
    uint64_t crc_errors;

    // callback do integracji z UI / innym klientem
    void *callback_user_data;
    void (*on_frame)(void *user_data,
                     const hw_protocol_frame_header_t *hdr,
                     const uint8_t *payload,
                     size_t payload_len);
} pico_session_t;

// Wynik operacji
typedef enum {
    PICO_OK = 0,
    PICO_ERR_TRANSPORT,   // błąd read/write
    PICO_ERR_TIMEOUT,     // brak czasu odpowiedzi
    PICO_ERR_PROTOCOL,    // zły magic, wersja, CRC
    PICO_ERR_STATE,       // niedozwolone w bieżącym stanie sesji
    PICO_ERR_DEVICE,      // Pico zwróciło MSG_ERROR
    PICO_ERR_OVERFLOW,    // bufor RX przepełniony
} pico_result_t;

pico_result_t transport_open(pico_session_t *s, const char *port, int baud);
void          transport_close(pico_session_t *s);

pico_result_t transport_write(pico_session_t *s, const uint8_t *buf, size_t len);

pico_result_t transport_read(pico_session_t *s);

uint16_t frame_crc16(const uint8_t *data, size_t len);

/*
 * Zwraca całkowitą liczbę bajtów do wysłania.
 */
size_t frame_build(pico_session_t *s, msg_type_t type,
                   const uint8_t *payload, uint16_t payload_len,
                   uint8_t *out_buf, size_t out_cap);

typedef enum {
    PARSE_OK,
    PARSE_NEED_MORE,   // za mało danych
    PARSE_BAD_MAGIC,   // uszkodzony stream, wyrzuć 1 bajt
    PARSE_BAD_CRC,     // ramka kompletna ale CRC złe
} parse_result_t;

/*
 * Wypełnia *hdr i ustawia *payload_out na wskaźnik do danych w rx_buf.
 * *consumed — ile bajtów zużyto (przesuń rx_buf o tyle po wywołaniu).
 */
parse_result_t frame_parse(const uint8_t *buf, size_t len,
                           hw_protocol_frame_header_t *hdr,
                           const uint8_t **payload_out,
                           size_t *consumed);

// komendy PC->Pico
pico_result_t session_hello(pico_session_t *s);
pico_result_t session_get_caps(pico_session_t *s);
pico_result_t session_get_status(pico_session_t *s);
pico_result_t session_set_bus(pico_session_t *s, const hw_protocol_set_bus_t *cfg);
pico_result_t session_set_target(pico_session_t *s, const hw_protocol_set_target_t *cfg);
pico_result_t session_set_fuzz_policy(pico_session_t *s, const hw_protocol_set_fuzz_policy_t *pol);
pico_result_t session_queue_stimulus(pico_session_t *s, uint32_t id,
                                      const uint8_t *data, uint16_t len,
                                      uint8_t flags, uint8_t kind);
pico_result_t session_arm(pico_session_t *s);
pico_result_t session_start_capture(pico_session_t *s);
pico_result_t session_start_fuzz(pico_session_t *s);
pico_result_t session_stop(pico_session_t *s);
pico_result_t session_disarm(pico_session_t *s);
pico_result_t session_reset(pico_session_t *s);

pico_result_t session_pump(pico_session_t *s);

// CSV
int  csv_open(pico_session_t *s, const char *path);
void csv_log_trace(pico_session_t *s, const hw_protocol_frame_header_t *hdr,
                   const hw_protocol_trace_decoded_t *tr);
void csv_close(pico_session_t *s);

// Diagnostyka
const char *msg_type_name(msg_type_t t);
const char *state_name(hw_protocol_session_state_t st);
void        frame_dump(const hw_protocol_frame_header_t *hdr,
                       const uint8_t *payload, size_t payload_len);

#endif // PICO_HOST_H
