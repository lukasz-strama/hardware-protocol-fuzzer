/**
 * @file usb_transport.c
 * @brief Warstwa transportowa USB oparta na TinyUSB (CDC) dla protokołu sniffera.
 *
 * Moduł odpowiada za:
 * - inicjalizację TinyUSB i parsera protokołu,
 * - obsługę zadań USB (tud_task),
 * - wysyłanie ramek protokołu przez CDC,
 * - odbiór bajtów z USB i przekazywanie ich do parsera,
 * - wywoływanie dispatcherów protokołu po zdekodowaniu ramki.
 *
 * Transport działa w trybie strumieniowym - każde odebrane bajty są
 * przekazywane do parsera protokołu.
 * 
 * Thread safety: Parser jest chroniony spin lock'iem dla synchronizacji
 * między USB ISR i główną pętlą.
 */
#include "usb_transport.h"
#include "protocol.h"
#include "protocol_layout.h"
#include "protocol_handlers_dispatcher.h"
#include "tusb.h"
#include "pico/sync.h"
#include <stdint.h>

static protocol_parser_t g_parser;
static spin_lock_t *g_parser_lock = NULL;

/**
 * @brief Inicjalizuje warstwę transportową USB.
 *
 * Funkcja:
 * - resetuje parser protokołu,
 * - inicjalizuje spin lock dla parsera,
 * - inicjalizuje stos TinyUSB.
 *
 * @note Musi być wywołana przed użyciem transportu.
 */
void usb_transport_init(void) {
    protocol_parser_init(&g_parser);
    
    /* Initialize spin lock for parser thread safety */
    g_parser_lock = spin_lock_init(spin_lock_claim_unused(true));
    
    tusb_init();
}

/**
 * @brief Obsługuje zadania USB.
 *
 * Funkcja powinna być wywoływana cyklicznie w pętli głównej.
 * tinyusb wymaga regularnego wywoływania tud_task().
 */
void usb_transport_task(void) {
    tud_task();
}

/**
 * @brief Wysyła dane przez interfejs CDC.
 *
 * Funkcja:
 * - sprawdza, czy host jest podłączony,
 * - wysyła dane porcjami zgodnie z dostępnością bufora CDC,
 * - wywołuje tud_task() w przypadku braku miejsca.
 *
 * @param data Bufor danych.
 * @param len Liczba bajtów do wysłania.
 */
void usb_transport_send(const uint8_t *data, size_t len) {
    if (!tud_cdc_connected()) return;

    size_t sent = 0;
    while (sent < len) {
        uint32_t available = tud_cdc_write_available();
        if (available == 0) {
            tud_task();
            continue;
        }
        uint32_t chunk = (len - sent) < available ? (len - sent) : available;
        sent += tud_cdc_write(data + sent, chunk);
    }
    tud_cdc_write_flush();
}

/**
 * @brief Przetwarza pojedynczy bajt odebrany z USB.
 *
 * Bajt jest przekazywany do parsera protokołu (chronionego spin lock'iem).  
 * Jeśli parser zdekoduje kompletną ramkę:
 * - wywoływany jest dispatcher protokołu (protocol_handle_frame).
 *
 * @param byte Odebrany bajt.
 */
void usb_transport_on_rx_byte(uint8_t byte) {
    hw_protocol_frame_header_t header;
    uint8_t payload[HW_PROTOCOL_MAX_TRACE_CHUNK];

    /* Acquire lock to protect parser from concurrent access */
    uint32_t save = spin_lock_blocking(g_parser_lock);
    
    bool frame_complete = protocol_parse_byte(&g_parser, byte, &header, payload);
    
    /* Release lock */
    spin_unlock(g_parser_lock, save);
    
    /* Call dispatcher outside of critical section */
    if (frame_complete) {
        protocol_handle_frame(&header, payload);
    }
}

/**
 * @brief Callback TinyUSB wywoływany przy odebraniu danych CDC.
 *
 * Funkcja:
 * - odczytuje dostępne bajty z endpointu CDC,
 * - przekazuje każdy bajt do usb_transport_on_rx_byte().
 *
 * @param itf Numer interfejsu CDC (ignorowany).
 */
void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
    uint8_t buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    for (uint32_t i = 0; i < count; i++) {
        usb_transport_on_rx_byte(buf[i]);
    }
}