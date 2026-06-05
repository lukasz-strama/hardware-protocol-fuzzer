/**
 * @file protocol_handlers_dispatcher.c
 * @brief Dispatcher komend protokołu, kieruje odebrane ramki
 *        do odpowiednich handlerów.
 *
 * Moduł jest centralnym punktem obsługi protokołu. Na podstawie
 * pola `type` w nagłówku ramki wybiera właściwą funkcję handlera
 * (np. GET_CAPS, ARM, START_CAPTURE, FUZZ, STOP, RESET_SESSION).
 *
 * Dispatcher nie interpretuje danych.
 */
#include "protocol_handlers_dispatcher.h"
#include "protocol.h"
#include "protocol_handlers.h"
#include "session.h"
#include <stdint.h>

/**
 * @brief Przetwarza odebraną ramkę protokołu i wywołuje odpowiedni handler.
 *
 * Funkcja analizuje pole `type` w nagłówku ramki i kieruje ją do
 * właściwej funkcji obsługującej daną komendę. Obsługiwane są m.in.:
 *
 * - MSG_TYPE_HELLO
 * - MSG_TYPE_GET_CAPS
 * - MSG_TYPE_GET_STATUS
 * - MSG_TYPE_SET_BUS
 * - MSG_TYPE_SET_TARGET
 * - MSG_TYPE_ARM
 * - MSG_TYPE_START_CAPTURE
 * - MSG_TYPE_SET_FUZZ_POLICY
 * - MSG_TYPE_QUEUE_STIMULUS
 * - MSG_TYPE_START_FUZZ
 * - MSG_TYPE_STOP
 * - MSG_TYPE_DISARM
 * - MSG_TYPE_RESET_SESSION
 *
 * Nieznane typy są przekazywane do handle_unknown().
 *
 * @param hdr Nagłówek ramki protokołu.
 * @param payload Dane ramki (może być NULL dla komend bez payloadu).
 *
 * @note Funkcja nie wykonuje walidacji payloadu — robią to poszczególne handlery.
 * @note Dispatcher zakłada, że ramka została poprawnie zdekodowana przez warstwę transportową.
 */
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
