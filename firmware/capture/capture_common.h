/**
 * @file capture_common.h
 * @brief Deklaracje funkcji wspólnych dla modułów przechwytujących
 *        dane z magistral I2C oraz UART.
 */
#ifndef CAPTURE_COMMON_H
#define CAPTURE_COMMON_H

#include "session.h"

/**
 * @brief Przygotowuje wybrany sniffer (I2C lub UART) do pracy.
 *
 * @param session Konfiguracja sesji sniffera.
 */
void capture_prepare(const sniffer_session_t *session);

/**
 * @brief Zatrzymuje aktywny sniffer i zwalnia zasoby.
 */
void capture_stop(void);

/**
 * @brief Główna funkcja wykonywana cyklicznie - przetwarza dane z aktywnej magistrali.
 */
void capture_task(void);

#endif // CAPTURE_COMMON_H