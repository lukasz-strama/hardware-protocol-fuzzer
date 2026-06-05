/**
 * @file capture_common.c
 * @brief Wspólna logika dla modułów przechwytujących dane z magistral
 *        I2C oraz UART. Zarządza przygotowaniem, obsługą i zatrzymaniem
 *        aktywnego sniffera.
 */

#include "capture_common.h"
#include "capture_i2c.h"
#include "capture_uart.h"

/**
 * @brief Główna funkcja wykonywana cyklicznie w pętli głównej.
 *
 * W zależności od aktywnej magistrali wywołuje odpowiedni moduł
 * przechwytujący:
 * - I2C - capture_i2c_poll()
 * - UART - capture_uart_poll()
 */
void capture_task(void) {
    if (g_session.active_bus == TARGET_BUS_I2C) {
       capture_i2c_poll();
    } else if (g_session.active_bus == TARGET_BUS_UART) {
        capture_uart_poll();
    }
}

/**
 * @brief Przygotowuje wybrany sniffer (I2C lub UART) do pracy.
 *
 * Funkcja inicjalizuje odpowiedni moduł przechwytujący na podstawie
 * konfiguracji przekazanej w strukturze sesji.
 *
 * @param session Wskaźnik na strukturę konfiguracji sesji sniffera.
 *
 * @note Funkcja powinna być wywołana przed rozpoczęciem przechwytywania.
 */
void capture_prepare(const sniffer_session_t *session) {
    if (session->active_bus == TARGET_BUS_I2C) {
        capture_i2c_init(session->i2c_sda_pin, session->i2c_scl_pin);
        capture_i2c_start();
    } else if (session->active_bus == TARGET_BUS_UART) {
        capture_uart_init(session->uart_rx_pin, session->uart_baudrate);
        capture_uart_start();
    }
}

/**
 * @brief Zatrzymuje aktywny sniffer i zwalnia zasoby.
 *
 * Funkcja wyłącza odpowiedni moduł przechwytujący oraz dezaktywuje
 * powiązane przerwania i PIO/SM.
 *
 * @note Powinna być wywoływana przy zmianie magistrali lub kończeniu sesji.
 */
void capture_stop(void) {
      if (g_session.active_bus == TARGET_BUS_I2C) {
        capture_i2c_stop();
    } else if (g_session.active_bus == TARGET_BUS_UART) {
        capture_uart_stop();
    }
}