/**
 * @file trace_encoder.c
 * @brief Implementacja enkodera zdarzeń śledzenia (trace) dla snifferów I2C/UART.
 *
 * Moduł buduje ramkę typu TRACE_DECODED zgodną z protokołem urządzenia
 * i wysyła ją przez warstwę transportową (protocol_send_frame()).
 *
 * Obsługiwane zdarzenia obejmują:
 * - bajty danych,
 * - warunki START/STOP,
 * - ACK/NACK,
 * - BREAK (UART),
 * - overflow FIFO PIO.
 */
#include "trace_encoder.h"
#include "protocol.h"
#include "protocol_layout.h"
#include "session.h"
#include <stdint.h>
#include <string.h>

/** Globalny licznik sekwencyjny ramek trace. */
static uint32_t trace_seq = 0;

/**
 * Bufor roboczy dla ramki TRACE_DECODED.
 * Maksymalna długość danych to 256 bajtów.
 */
static uint8_t trace_buf[sizeof(hw_protocol_trace_decoded_t) + 256];

/**
 * @brief Koduje i wysyła zdarzenie trace.
 *
 * Funkcja buduje ramkę `hw_protocol_trace_decoded_t` zgodną z protokołem
 * i wysyła ją przez warstwę transportową. Zdarzenie może zawierać:
 * - znacznik czasu,
 * - typ magistrali (I2C/UART),
 * - typ zdarzenia (BYTE, START, STOP, ACK, NACK, BREAK, OVERFLOW),
 * - opcjonalne dane (np. bajt odebrany z magistrali).
 *
 * @param timestamp_us Znacznik czasu w mikrosekundach.
 * @param source_bus Kod magistrali (TRACE_SOURCE_I2C lub TRACE_SOURCE_UART).
 * @param event_type Typ zdarzenia (TRACE_EVENT_xxx).
 * @param data Wskaźnik na dane (może być NULL).
 * @param len Liczba bajtów danych (maks. 256).
 *
 * @note Funkcja automatycznie ogranicza długość danych do 256 bajtów.
 * @note trace_seq jest inkrementowane przy każdej ramce.
 */
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