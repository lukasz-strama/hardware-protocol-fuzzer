#ifndef HW_PROTOCOL_LAYOUT_H
#define HW_PROTOCOL_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Układy binarne po stronie hosta dla protokołu v1.
 *
 * Nie używaj __packed. Kolejność pól została dobrana tak, aby stałe części
 * payloadów pozostawały naturalnie wyrównane na typowych ABI ARM i desktopu.
 * Deserializuj te typy dopiero po skopiowaniu bajtów do wyrównanego bufora
 * albo przez jawne dekodowanie pól.
 */

#define HW_PROTOCOL_VERSION_V1 1u
#define HW_PROTOCOL_HEADER_SIZE 16u
#define HW_PROTOCOL_MAX_TRACE_CHUNK 1024u
#define HW_PROTOCOL_MAX_PENDING_STIMULI 32u
#define HW_PROTOCOL_MAX_PENDING_STIMULUS_BYTES 4096u
#define HW_PROTOCOL_ARM_TIMEOUT_MS 500u
#define HW_PROTOCOL_STOP_TIMEOUT_MS 200u

#if defined(__cplusplus)
#define HW_PROTOCOL_STATIC_ASSERT static_assert
#else
#define HW_PROTOCOL_STATIC_ASSERT _Static_assert
#endif

/* Stany sesji. */
typedef enum hw_protocol_session_state {
    HW_PROTOCOL_STATE_DETACHED = 0,
    HW_PROTOCOL_STATE_CONNECTED = 1,
    HW_PROTOCOL_STATE_CAPABILITIES_READ = 2,
    HW_PROTOCOL_STATE_CONFIGURED = 3,
    HW_PROTOCOL_STATE_ARMED = 4,
    HW_PROTOCOL_STATE_RUNNING = 5,
    HW_PROTOCOL_STATE_STOPPING = 6,
    HW_PROTOCOL_STATE_FAULT = 7,
} hw_protocol_session_state_t;

/* Typy magistrali. */
typedef enum hw_protocol_bus_type {
    HW_PROTOCOL_BUS_I2C = 0,
    HW_PROTOCOL_BUS_UART = 1,
} hw_protocol_bus_type_t;

/* Klasy zdarzeń raportowane w zdekodowanych śladach. */
typedef enum hw_protocol_event_type {
    HW_PROTOCOL_EVENT_BYTE = 0,
    HW_PROTOCOL_EVENT_START = 1,
    HW_PROTOCOL_EVENT_STOP = 2,
    HW_PROTOCOL_EVENT_ACK = 3,
    HW_PROTOCOL_EVENT_NACK = 4,
    HW_PROTOCOL_EVENT_BREAK = 5,
    HW_PROTOCOL_EVENT_OVERFLOW = 6,
} hw_protocol_event_type_t;

/* Poziomy ważności raportów błędów. */
typedef enum hw_protocol_severity {
    HW_PROTOCOL_SEVERITY_INFO = 0,
    HW_PROTOCOL_SEVERITY_WARNING = 1,
    HW_PROTOCOL_SEVERITY_ERROR = 2,
    HW_PROTOCOL_SEVERITY_FATAL = 3,
} hw_protocol_severity_t;

/* Flagi możliwości i polityk. */
enum {
    HW_PROTOCOL_CLIENT_WANTS_FUZZ = 1u << 0,
    HW_PROTOCOL_CLIENT_WANTS_STREAMING = 1u << 1,
    HW_PROTOCOL_CLIENT_WANTS_CAPTURE = 1u << 2,

    HW_PROTOCOL_FW_SUPPORTS_FUZZ = 1u << 0,
    HW_PROTOCOL_FW_SUPPORTS_STREAMING = 1u << 1,
    HW_PROTOCOL_FW_REQUIRES_EXTERNAL_PULLUPS = 1u << 2,

    HW_PROTOCOL_MODE_CAPTURE = 1u << 0,
    HW_PROTOCOL_MODE_FUZZ = 1u << 1,

    HW_PROTOCOL_STIMULUS_INLINE_PAYLOAD = 1u << 0,
    HW_PROTOCOL_STIMULUS_GENERATED = 1u << 1,
    HW_PROTOCOL_STIMULUS_LAST_IN_SEQUENCE = 1u << 2,

    HW_PROTOCOL_PULLUP_NONE = 0u,
    HW_PROTOCOL_PULLUP_EXTERNAL = 1u,
    HW_PROTOCOL_PULLUP_INTERNAL_TEST_ONLY = 2u,
};

typedef struct hw_protocol_frame_header {
    uint8_t magic[2];
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t padding;
    uint16_t session_id;
    uint32_t sequence;
    uint16_t length;
    uint16_t checksum;
} hw_protocol_frame_header_t;

typedef struct hw_protocol_hello {
    uint8_t requested_protocol;
    uint8_t client_flags;
    uint16_t client_version;
    uint16_t reserved;
} hw_protocol_hello_t;

typedef struct hw_protocol_hello_ack {
    uint8_t negotiated_protocol;
    uint8_t fw_flags;
    uint16_t fw_version;
    uint16_t session_id;
    uint16_t reserved;
} hw_protocol_hello_ack_t;

typedef struct hw_protocol_set_bus {
    uint32_t speed_hz;
    uint8_t bus_type;
    uint8_t bus_flags;
    uint8_t pin_a;
    uint8_t pin_b;
    uint8_t uart_parity;
    uint8_t uart_stop_bits;
    uint8_t reserved;
    uint8_t reserved2;
} hw_protocol_set_bus_t;

typedef struct hw_protocol_set_target {
    uint16_t vtarget_mv;
    uint8_t pin_dir_mask;
    uint8_t pullup_mode;
    uint8_t pullup_mask;
    uint8_t reserved;
    uint16_t reserved2;
} hw_protocol_set_target_t;

typedef struct hw_protocol_set_fuzz_policy {
    uint32_t time_budget_ms;
    uint16_t pending_bytes;
    uint8_t policy_flags;
    uint8_t selection_mode;
    uint8_t repeat_mode;
    uint8_t max_pending;
    uint8_t reserved;
    uint8_t reserved2;
} hw_protocol_set_fuzz_policy_t;

typedef struct hw_protocol_queue_stimulus {
    uint32_t stimulus_id;
    uint16_t data_len;
    uint8_t stimulus_flags;
    uint8_t stimulus_kind;
    uint8_t data[];
} hw_protocol_queue_stimulus_t;

typedef struct hw_protocol_arm {
    uint16_t session_id;
    uint8_t arm_flags;
    uint8_t reserved;
} hw_protocol_arm_t;

typedef struct hw_protocol_status {
    uint32_t rx_overruns;
    uint32_t tx_underruns;
    uint32_t armed_since_ms;
    uint16_t session_id;
    uint16_t last_error;
    uint8_t state;
    uint8_t flags;
    uint8_t queued_stimuli;
    uint8_t reserved;
} hw_protocol_status_t;

typedef struct hw_protocol_pin_map_entry {
    uint8_t gpio;
    uint8_t role;
    uint8_t capabilities;
    uint8_t reserved;
} hw_protocol_pin_map_entry_t;

typedef struct hw_protocol_caps_response {
    uint32_t buffer_bytes;
    uint16_t max_burst_bytes;
    uint8_t fw_version;
    uint8_t protocol_version;
    uint8_t bus_mask;
    uint8_t supported_modes;
    uint8_t pio_sm_count;
    uint8_t reserved;
    uint8_t pin_map_count;
    hw_protocol_pin_map_entry_t pin_map[];
} hw_protocol_caps_response_t;

typedef struct hw_protocol_trace_decoded {
    uint32_t trace_seq;
    uint32_t timestamp_us;
    uint16_t data_len;
    uint8_t source_bus;
    uint8_t event_type;
    uint8_t data[];
} hw_protocol_trace_decoded_t;

typedef struct hw_protocol_fuzz_tx {
    uint32_t stimulus_id;
    uint32_t trace_seq;
    uint8_t mode;
    uint8_t flags;
    uint16_t data_len;
    uint8_t data[];
} hw_protocol_fuzz_tx_t;

typedef struct hw_protocol_arm_ok {
    uint16_t session_id;
    uint8_t state;
    uint8_t reserved;
} hw_protocol_arm_ok_t;

typedef struct hw_protocol_stop_ok {
    uint32_t drained_bytes;
    uint16_t session_id;
    uint8_t state;
    uint8_t reserved;
} hw_protocol_stop_ok_t;

typedef struct hw_protocol_error {
    uint16_t context_code;
    uint16_t error_code;
    uint16_t message_len;
    uint8_t severity;
    uint8_t reserved;
    uint8_t message[];
} hw_protocol_error_t;

HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_frame_header_t) == 16, "frame header must be 16 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_hello_t) == 6, "hello payload must be 6 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_hello_ack_t) == 8, "hello ack payload must be 8 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_set_bus_t) == 12, "set_bus payload must be 12 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_set_target_t) == 8, "set_target payload must be 8 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_set_fuzz_policy_t) == 12, "set_fuzz_policy payload must be 12 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_queue_stimulus_t) == 8, "queue_stimulus fixed part must be 8 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_arm_t) == 4, "arm payload must be 4 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_status_t) == 20, "status payload must be 20 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_caps_response_t) == 12, "caps response fixed part must be 12 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_trace_decoded_t) == 12, "trace decoded fixed part must be 12 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_fuzz_tx_t) == 12, "fuzz tx fixed part must be 12 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_arm_ok_t) == 4, "arm ok payload must be 4 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_stop_ok_t) == 8, "stop ok payload must be 8 bytes");
HW_PROTOCOL_STATIC_ASSERT(sizeof(hw_protocol_error_t) == 8, "error fixed part must be 8 bytes");

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* HW_PROTOCOL_LAYOUT_H */
