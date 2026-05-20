#ifndef USB_TRANSPORT_H
#define USB_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void usb_transport_init(void);
void usb_transport_task(void);
void usb_transport_send(const uint8_t *data, size_t len);
void usb_transport_on_rx_byte(uint8_t byte);

#endif