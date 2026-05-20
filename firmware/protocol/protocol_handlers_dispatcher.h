#ifndef PROTOCOL_HANDLERS_DISPATCHER_H
#define PROTOCOL_HANDLERS_DISPATCHER_H

#include "protocol_layout.h"
#include <stdint.h>

void protocol_handle_frame(const hw_protocol_frame_header_t *hdr, const uint8_t *payload);

#endif
