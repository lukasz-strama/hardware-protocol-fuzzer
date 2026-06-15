/**
 * @file session.h
 * @brief Struktury i funkcje zarządzające stanem sesji sniffera.
 *
 * Sesja obejmuje:
 * - konfigurację magistrali,
 * - konfigurację targetu,
 * - przejścia stanów protokołu,
 * - obsługę capture (I2C/UART),
 * - obsługę fuzzingu (UART).
 */
#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_layout.h"

/**
 * @brief Timeouts (in microseconds).
 */
#define SESSION_ARM_TIMEOUT_US (500 * 1000)      /* 500 ms */
#define SESSION_STOP_TIMEOUT_US (200 * 1000)     /* 200 ms */

/**
 * @brief Typ magistrali aktywnej w sesji.
 */
typedef enum {
    TARGET_BUS_NONE = 0,
    TARGET_BUS_I2C  = 1,
    TARGET_BUS_UART = 2
} target_bus_t;

/**
 * @brief Struktura przechowująca pełny stan sesji sniffera.
 */
typedef struct {
    hw_protocol_session_state_t current_state;
    hw_protocol_status_t status;   
    uint16_t session_id;
    target_bus_t active_bus;

    uint32_t uart_baudrate;
    uint8_t  uart_tx_pin;
    uint8_t  uart_rx_pin;

    uint32_t i2c_frequency_khz;
    uint8_t  i2c_sda_pin;
    uint8_t  i2c_scl_pin;

    uint16_t vtarget_mv;
    uint8_t  pullup_mode;
    uint8_t  pullup_mask;

    bool     fuzz_mode;
    bool     fuzz_policy_ready;
    hw_protocol_set_fuzz_policy_t fuzz_policy;

    /* Timeout tracking (microseconds since boot) */
    uint64_t stop_timeout_start;    /* When STOP was received */
    uint32_t armed_since_us;        /* Timestamp when ARM completed */
} sniffer_session_t;

/** Globalna instancja sesji. */
extern sniffer_session_t g_session;

void     session_init(void);
void     session_handle_set_bus(const uint8_t *payload);
void     session_handle_set_target(const uint8_t *payload);
void     session_handle_arm(uint16_t session_id);
void     session_handle_start_capture(void);
bool     session_handle_set_fuzz_policy(const uint8_t *payload, uint16_t len);
bool     session_handle_start_fuzz(void);
uint32_t session_handle_stop(void);
void     session_handle_disarm(void);

/**
 * @brief Check for timeout violations (ARM, STOP).
 * 
 * Must be called periodically from main loop.
 * Transitions to FAULT if timeout exceeded.
 */
void     session_check_timeouts(void);

#endif