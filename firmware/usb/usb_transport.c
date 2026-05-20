#include "usb_transport.h"
#include "protocol.h"
#include "protocol_layout.h"
#include "protocol_handlers_dispatcher.h"
#include "tusb.h"
#include <stdint.h>

static protocol_parser_t g_parser;

void usb_transport_init(void) {
    protocol_parser_init(&g_parser);
    tusb_init();
}

void usb_transport_task(void) {
    tud_task();
}

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

void usb_transport_on_rx_byte(uint8_t byte) {
    hw_protocol_frame_header_t header;
    uint8_t payload[HW_PROTOCOL_MAX_TRACE_CHUNK];

    if (protocol_parse_byte(&g_parser, byte, &header, payload)) {
        protocol_handle_frame(&header, payload);
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
    uint8_t buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    for (uint32_t i = 0; i < count; i++) {
        usb_transport_on_rx_byte(buf[i]);
    }
}