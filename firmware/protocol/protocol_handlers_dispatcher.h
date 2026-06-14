/**
 * @file protocol_handlers_dispatcher.h
 * @brief Deklaracje funkcji odpowiedzialnych za dispatching komend protokołu.
 *
 * Dispatcher analizuje nagłówek ramki i wywołuje odpowiedni handler
 * implementujący logikę danej komendy.
 */
#ifndef PROTOCOL_HANDLERS_DISPATCHER_H
#define PROTOCOL_HANDLERS_DISPATCHER_H

#include "protocol_layout.h"
#include <stdint.h>

/**
 * @brief Przetwarza ramkę protokołu i kieruje ją do właściwego handlera.
 *
 * @param hdr Nagłówek ramki protokołu.
 * @param payload Dane ramki (może być NULL).
 */
void protocol_handle_frame(const hw_protocol_frame_header_t *hdr, const uint8_t *payload);

#endif
