/**
 * @file protocol_handlers.c
 * @brief Implementacja handlerów komend protokołu urządzenia.
 *
 * Każda funkcja w tym module odpowiada za obsługę konkretnego typu
 * wiadomości protokołu. Handlery wykonują logikę biznesową, aktualizują
 * stan sesji oraz wysyłają odpowiednie ramki odpowiedzi.
 */
#include "protocol_handlers.h"
#include "protocol.h"
#include "protocol_layout.h"
#include "session.h"
#include "fuzz_engine.h"
#include <string.h>

#ifndef HW_PROTOCOL_FW_SUPPORTS_CAPTURE
#define HW_PROTOCOL_FW_SUPPORTS_CAPTURE (HW_PROTOCOL_FW_SUPPORTS_STREAMING)
#endif

/**
 * @brief Obsługuje komendę GET_CAPS, zwraca możliwości firmware.
 *
 * Zwracane informacje obejmują:
 * - rozmiary buforów,
 * - wersję firmware i protokołu,
 * - obsługiwane magistrale,
 * - obsługiwane tryby (capture, fuzz),
 * - liczbę dostępnych state machine PIO.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 */
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

/**
 * @brief Obsługuje komendę GET_STATUS, zwraca aktualny stan sesji.
 *
 * Zwracane informacje obejmują:
 * - stan maszyny stanów,
 * - liczbę zakolejkowanych bodźców fuzz,
 * - błędy i flagi statusowe.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 */
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

/**
 * @brief Obsługuje komendę HELLO, negocjuje wersję protokołu.
 *
 * Ustawia stan sesji na CONNECTED i odsyła ramkę HELLO_ACK.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 * @param payload Nie używane.
 * @param len Nie używane.
 */
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

/**
 * @brief Obsługuje komendę ARM, przygotowuje urządzenie do pracy.
 *
 * Ustawia stan sesji na ARMED i odsyła ARM_OK.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 * @param payload Nie używane.
 * @param len Nie używane.
 */
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

/**
 * @brief Obsługuje komendę START_CAPTURE - uruchamia przechwytywanie.
 *
 * Po uruchomieniu capture odsyła aktualny STATUS.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 */
void handle_start_capture(uint16_t session_id, uint32_t seq) {
    session_handle_start_capture();
    handle_get_status(session_id, seq);
}

/**
 * @brief Obsługuje komendę SET_FUZZ_POLICY, ustawia politykę fuzzingu.
 *
 * W przypadku błędu odsyła ramkę ERROR.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 * @param payload Dane polityki fuzz.
 * @param len Długość danych.
 */
void handle_set_fuzz_policy(uint16_t session_id, uint32_t seq,
                             const uint8_t *payload, uint16_t len)
{
    bool ok = session_handle_set_fuzz_policy(payload, len);
    if (!ok) {
        hw_protocol_error_t err = {
            .context_code = MSG_TYPE_SET_FUZZ_POLICY,
            .error_code   = 1,
            .message_len  = 0,
            .severity     = HW_PROTOCOL_SEVERITY_ERROR,
            .reserved     = 0
        };
        protocol_send_frame(MSG_TYPE_ERROR, session_id, seq,
                            (const uint8_t *)&err, sizeof(err));
        return;
    }
    handle_get_status(session_id, seq);
}

/**
 * @brief Obsługuje komendę QUEUE_STIMULUS, dodaje bodziec fuzz do kolejki.
 *
 * Waliduje:
 * - czy urządzenie nie jest w stanie RUNNING,
 * - czy kolejka fuzz nie jest pełna.
 *
 * W przypadku błędu odsyła ramkę ERROR.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 * @param payload Dane bodźca.
 * @param len Długość danych.
 */
void handle_queue_stimulus(uint16_t session_id, uint32_t seq,
                            const uint8_t *payload, uint16_t len)
{
    if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING) {
        hw_protocol_error_t err = {
            .context_code = MSG_TYPE_QUEUE_STIMULUS,
            .error_code   = 2,  /* rejected: running */
            .message_len  = 0,
            .severity     = HW_PROTOCOL_SEVERITY_ERROR,
            .reserved     = 0
        };
        protocol_send_frame(MSG_TYPE_ERROR, session_id, seq,
                            (const uint8_t *)&err, sizeof(err));
        return;
    }
    bool ok = fuzz_engine_queue_stimulus(payload, len);
    if (!ok) {
        hw_protocol_error_t err = {
            .context_code = MSG_TYPE_QUEUE_STIMULUS,
            .error_code   = 3,  /* queue full or pool exhausted */
            .message_len  = 0,
            .severity     = HW_PROTOCOL_SEVERITY_ERROR,
            .reserved     = 0
        };
        protocol_send_frame(MSG_TYPE_ERROR, session_id, seq,
                            (const uint8_t *)&err, sizeof(err));
        return;
    }
    /* Update STATUS so desktop can track queue depth */
    g_session.status.queued_stimuli = fuzz_engine_queued_count();
    handle_get_status(session_id, seq);
}

/**
 * @brief Obsługuje komendę START_FUZZ - uruchamia fuzzing.
 *
 * Po uruchomieniu odsyła STATUS.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 */
void handle_start_fuzz(uint16_t session_id, uint32_t seq) {
    session_handle_start_fuzz();
    handle_get_status(session_id, seq);
}

/**
 * @brief Obsługuje komendę STOP - zatrzymuje capture lub fuzzing.
 *
 * Zwraca liczbę opróżnionych bajtów (drained_bytes).
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 */
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

/**
 * @brief Obsługuje komendę DISARM, dezaktywuje urządzenie.
 *
 * Po dezaktywacji odsyła STATUS.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 */
void handle_disarm(uint16_t session_id, uint32_t seq)
{
    session_handle_disarm();
    handle_get_status(session_id, seq);
}

/**
 * @brief Obsługuje komendę RESET_SESSION - resetuje stan sesji.
 *
 * @param session_id Ignorowane.
 * @param seq Ignorowane.
 */
void handle_reset_session(uint16_t session_id, uint32_t seq)
{
    (void)session_id;
    (void)seq;
    session_init();
}

/**
 * @brief Obsługuje nieznane typy wiadomości - odsyła ramkę ERROR.
 *
 * @param session_id Identyfikator sesji.
 * @param seq Numer sekwencyjny ramki.
 * @param type Nieznany typ wiadomości.
 */
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
