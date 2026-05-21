#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_layout.h"

typedef enum {
    TARGET_BUS_NONE = 0,
    TARGET_BUS_I2C  = 1,
    TARGET_BUS_UART = 2
} target_bus_t;

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
} sniffer_session_t;

extern sniffer_session_t g_session;

void     session_init(void);
void     session_handle_set_bus(const uint8_t *payload);
void     session_handle_set_target(const uint8_t *payload);
void     session_handle_arm(uint16_t session_id);
void     session_handle_start_capture(void);
uint32_t session_handle_stop(void);
void     session_handle_disarm(void);  

#endif