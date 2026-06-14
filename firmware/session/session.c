/**
 * @file session.c
 * @brief Implementacja maszyny stanów sesji sniffera (capture + fuzz).
 *
 * Moduł zarządza:
 * - konfiguracją magistrali (SET_BUS),
 * - konfiguracją targetu (SET_TARGET),
 * - przejściami stanów (CONNECTED - CONFIGURED - ARMED - RUNNING),
 * - obsługą capture (I2C/UART),
 * - obsługą fuzzingu (tylko UART),
 * - resetowaniem i dezaktywowaniem sesji.
 *
 * Struktura g_session przechowuje pełny stan urządzenia.
 */
#include "session.h"
#include "capture_common.h"
#include "fuzz_engine.h"
#include "pico/stdlib.h"
#include <string.h>

/** Globalna instancja sesji sniffera. */
sniffer_session_t g_session;

/**
 * @brief Resetuje całą sesję do stanu początkowego.
 *
 * Ustawia:
 * - current_state = DETACHED,
 * - active_bus = NONE,
 * - wyłącza tryb fuzz,
 * - resetuje politykę fuzz,
 * - inicjalizuje fuzz_engine.
 */
void session_init(void) {
    memset(&g_session, 0, sizeof(g_session));
    g_session.current_state   = HW_PROTOCOL_STATE_DETACHED;
    g_session.active_bus      = TARGET_BUS_NONE;
    g_session.fuzz_mode       = false;
    g_session.fuzz_policy_ready = false;
    fuzz_engine_init();
}

/**
 * @brief Obsługuje komendę SET_BUS - wybór magistrali i konfiguracja pinów.
 *
 * Ustawia:
 * - aktywną magistralę (UART/I2C),
 * - piny magistrali,
 * - prędkość UART lub piny I2C.
 *
 * Po konfiguracji stan przechodzi do CONFIGURED.
 *
 * @param payload Dane komendy SET_BUS.
 */
void session_handle_set_bus(const uint8_t *payload) {
    const hw_protocol_set_bus_t *p = (const hw_protocol_set_bus_t *)payload;

    if (p->bus_type == HW_PROTOCOL_BUS_UART) {
        g_session.active_bus    = TARGET_BUS_UART;
        g_session.uart_baudrate = p->speed_hz;
        g_session.uart_rx_pin   = p->pin_b;
        g_session.uart_tx_pin   = p->pin_a;
    } else if (p->bus_type == HW_PROTOCOL_BUS_I2C) {
        g_session.active_bus  = TARGET_BUS_I2C;
        g_session.i2c_sda_pin = p->pin_a;
        g_session.i2c_scl_pin = p->pin_b;
    }

    g_session.current_state = HW_PROTOCOL_STATE_CONFIGURED;
}

/**
 * @brief Obsługuje komendę SET_TARGET - konfiguracja napięcia i pull-upów.
 *
 * @param p Payload komendy SET_TARGET.
 */
void session_handle_set_target(const uint8_t *p) {
    uint16_t vtarget_mv  = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    uint8_t  pullup_mode = p[3];
    uint8_t  pullup_mask = p[4];

    g_session.vtarget_mv  = vtarget_mv;
    g_session.pullup_mode = pullup_mode;
    g_session.pullup_mask = pullup_mask;

    if (g_session.current_state == HW_PROTOCOL_STATE_CONFIGURED ||
        g_session.current_state == HW_PROTOCOL_STATE_CONNECTED) {
        g_session.current_state = HW_PROTOCOL_STATE_CONFIGURED;
    }
}

/**
 * @brief Obsługuje komendę ARM - przygotowuje urządzenie do pracy.
 *
 * Wywołuje capture_prepare() i przechodzi do stanu ARMED.
 *
 * @param session_id Identyfikator sesji.
 */
void session_handle_arm(uint16_t session_id) {
    if (g_session.current_state != HW_PROTOCOL_STATE_CONFIGURED) return;
    g_session.session_id    = session_id;
    g_session.current_state = HW_PROTOCOL_STATE_ARMED;
    capture_prepare(&g_session);   
}

/**
 * @brief Obsługuje komendę START_CAPTURE - uruchamia przechwytywanie.
 */
void session_handle_start_capture(void) {
    if (g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
        g_session.current_state = HW_PROTOCOL_STATE_RUNNING;
    }
}

/**
 * @brief Obsługuje komendę STOP - zatrzymuje capture lub fuzzing.
 *
 * Zatrzymuje capture_stop() i wraca do stanu ARMED.
 *
 * @return Liczba opróżnionych bajtów (obecnie zawsze 0).
 */
uint32_t session_handle_stop(void) {
    if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING ||
        g_session.current_state == HW_PROTOCOL_STATE_ARMED) {
        g_session.current_state = HW_PROTOCOL_STATE_STOPPING;
        capture_stop();
        g_session.current_state = HW_PROTOCOL_STATE_ARMED;
    }
    return 0;
}

/**
 * @brief Obsługuje komendę SET_FUZZ_POLICY - ustawia politykę fuzzingu.
 *
 * Nie można zmieniać polityki w trakcie RUNNING.
 *
 * @param payload Dane polityki fuzz.
 * @param len Długość danych.
 * @return true jeśli polityka została zaakceptowana.
 */
bool session_handle_set_fuzz_policy(const uint8_t *payload, uint16_t len) {
    if (g_session.current_state == HW_PROTOCOL_STATE_RUNNING) return false;
    bool ok = fuzz_engine_set_policy(payload, len);
    if (ok) {
        g_session.fuzz_policy_ready = true;
    }
    return ok;
}

/**
 * @brief Obsługuje komendę START_FUZZ - uruchamia fuzzing.
 *
 * Warunki:
 * - stan = ARMED,
 * - polityka fuzz ustawiona,
 * - aktywna magistrala = UART.
 */
void session_handle_start_fuzz(void) {
    if (g_session.current_state != HW_PROTOCOL_STATE_ARMED) return;
    if (!g_session.fuzz_policy_ready) return;
    if (g_session.active_bus != TARGET_BUS_UART) return;

    g_session.fuzz_mode     = true;
    g_session.current_state = HW_PROTOCOL_STATE_RUNNING;
    fuzz_engine_start();
}

/**
 * @brief Obsługuje komendę DISARM - dezaktywuje urządzenie.
 *
 * Zatrzymuje fuzz_engine i capture, resetuje stan i wraca do CONNECTED.
 */
void session_handle_disarm(void) {
    fuzz_engine_stop();
    capture_stop();
    g_session.fuzz_mode         = false;
    g_session.fuzz_policy_ready = false;
    g_session.active_bus        = TARGET_BUS_NONE;
    g_session.current_state     = HW_PROTOCOL_STATE_CONNECTED;
}