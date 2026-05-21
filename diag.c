#include "pico_host.h"

const char *msg_type_name(msg_type_t t)
{
    switch (t) {
        case MSG_HELLO:            return "HELLO";
        case MSG_GET_CAPS:         return "GET_CAPS";
        case MSG_GET_STATUS:       return "GET_STATUS";
        case MSG_SET_BUS:          return "SET_BUS";
        case MSG_SET_TARGET:       return "SET_TARGET";
        case MSG_SET_FUZZ_POLICY:  return "SET_FUZZ_POLICY";
        case MSG_QUEUE_STIMULUS:   return "QUEUE_STIMULUS";
        case MSG_ARM:              return "ARM";
        case MSG_START_CAPTURE:    return "START_CAPTURE";
        case MSG_START_FUZZ:       return "START_FUZZ";
        case MSG_STOP:             return "STOP";
        case MSG_DISARM:           return "DISARM";
        case MSG_RESET_SESSION:    return "RESET_SESSION";
        case MSG_CAPS_RESPONSE:    return "CAPS_RESPONSE";
        case MSG_HELLO_ACK:        return "HELLO_ACK";
        case MSG_ARM_OK:           return "ARM_OK";
        case MSG_STOP_OK:          return "STOP_OK";
        case MSG_STATUS:           return "STATUS";
        case MSG_TRACE_DECODED:    return "TRACE_DECODED";
        case MSG_FUZZ_TX:          return "FUZZ_TX";
        case MSG_COUNTERS:         return "COUNTERS";
        case MSG_ERROR:            return "ERROR";
        default:                   return "UNKNOWN";
    }
}

const char *state_name(hw_protocol_session_state_t st)
{
    switch (st) {
        case HW_PROTOCOL_STATE_DETACHED:          return "Detached";
        case HW_PROTOCOL_STATE_CONNECTED:         return "Connected";
        case HW_PROTOCOL_STATE_CAPABILITIES_READ: return "CapabilitiesRead";
        case HW_PROTOCOL_STATE_CONFIGURED:        return "Configured";
        case HW_PROTOCOL_STATE_ARMED:             return "Armed";
        case HW_PROTOCOL_STATE_RUNNING:           return "Running";
        case HW_PROTOCOL_STATE_STOPPING:          return "Stopping";
        case HW_PROTOCOL_STATE_FAULT:             return "Fault";
        default:                                  return "Unknown";
    }
}

void frame_dump(const hw_protocol_frame_header_t *hdr,
                const uint8_t *payload, size_t payload_len)
{
    printf("  magic=%02X%02X ver=%u type=0x%02X(%s) flags=0x%02X "
           "session=0x%04X seq=%u len=%u crc=0x%04X\n",
           hdr->magic[0], hdr->magic[1],
           hdr->version, hdr->type, msg_type_name((msg_type_t)hdr->type),
           hdr->flags, hdr->session_id, hdr->sequence,
           hdr->length, hdr->checksum);

    if (payload_len) {
        printf("  payload[%zu]:", payload_len);
        size_t show = payload_len < 16 ? payload_len : 16;
        for (size_t i = 0; i < show; i++) printf(" %02X", payload[i]);
        if (payload_len > 16) printf(" ...");
        printf("\n");
    }
}
