/**
 * @file usb_transport.h
 * @brief Interfejs warstwy transportowej USB dla protokołu sniffera.
 *
 * Moduł zapewnia:
 * - inicjalizację USB,
 * - wysyłanie danych przez CDC,
 * - odbiór bajtów i integrację z parserem protokołu.
 */
#ifndef USB_TRANSPORT_H
#define USB_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicjalizuje transport USB i parser protokołu.
 */
void usb_transport_init(void);
/**
 * @brief Obsługuje zadania USB (tud_task).
 *
 * Powinna być wywoływana cyklicznie.
 */
void usb_transport_task(void);
/**
 * @brief Wysyła dane przez interfejs CDC.
 *
 * @param data Bufor danych.
 * @param len Liczba bajtów.
 */
void usb_transport_send(const uint8_t *data, size_t len);
/**
 * @brief Przetwarza pojedynczy bajt odebrany z USB.
 *
 * @param byte Odebrany bajt.
 */
void usb_transport_on_rx_byte(uint8_t byte);

#endif