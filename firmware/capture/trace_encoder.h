/**
 * @file trace_encoder.h
 * @brief Definicje typów zdarzeń trace oraz interfejs enkodera.
 *
 * Moduł służy do emitowania zdarzeń śledzenia z modułów przechwytujących
 * (I2C i UART). Każde zdarzenie jest pakowane do ramki TRACE_DECODED
 * i wysyłane przez warstwę protokołu.
 */
#ifndef TRACE_ENCODER_H
#define TRACE_ENCODER_H

/** Źródło zdarzenia: magistrala I2C. */
#define TRACE_SOURCE_I2C 0
/** Źródło zdarzenia: magistrala UART. */
#define TRACE_SOURCE_UART 1

/** Zdarzenie: odebrany bajt danych. */
#define TRACE_EVENT_BYTE 0
/** Zdarzenie: warunek START (I2C). */
#define TRACE_EVENT_START 1
/** Zdarzenie: warunek STOP (I2C). */
#define TRACE_EVENT_STOP 2
/** Zdarzenie: ACK (I2C). */
#define TRACE_EVENT_ACK 3
/** Zdarzenie: NACK (I2C). */
#define TRACE_EVENT_NACK 4
/** Zdarzenie: BREAK (UART). */
#define TRACE_EVENT_BREAK 5
/** Zdarzenie: overflow FIFO PIO. */
#define TRACE_EVENT_OVERFLOW 6

/**
 * @brief Koduje i wysyła zdarzenie trace.
 *
 * @param timestamp_us Znacznik czasu w mikrosekundach.
 * @param source_bus Kod magistrali (I2C/UART).
 * @param event_type Typ zdarzenia.
 * @param data Wskaźnik na dane (może być NULL).
 * @param len Liczba bajtów danych.
 */
void trace_emit(uint32_t timestamp_us,
                uint8_t source_bus,
                uint8_t event_type,
                const uint8_t *data,
                uint16_t len);

#endif
