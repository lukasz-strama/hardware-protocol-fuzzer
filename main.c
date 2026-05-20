#include "pico_host.h"

#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

// Sygnaly
static volatile sig_atomic_t g_stop = 0;
static void sig_handler(int s) { (void)s; g_stop = 1; }

static pico_result_t wait_for_state(pico_session_t *s,
                                     pico_result_t (*send_fn)(pico_session_t *),
                                     hw_protocol_session_state_t expected_state,
                                     int timeout_ms)
{
    pico_result_t r = send_fn(s);
    if (r) return r;

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec  += timeout_ms / 1000;
    deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    while (!g_stop) {
        r = session_pump(s);
        if (r) return r;
        if (s->state == expected_state) return PICO_OK;
        if (s->state == HW_PROTOCOL_STATE_FAULT) return PICO_ERR_DEVICE;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
            fprintf(stderr, "[main] Timeout while waiting for state %s\n",
                    state_name(expected_state));
            return PICO_ERR_TIMEOUT;
        }
        usleep(5000);   // 5 ms
    }
    return PICO_OK;
}

/* ─── Pomocnicze wrappery dla wait_for_state ────────────────────── */
static pico_result_t do_hello(pico_session_t *s)   { return session_hello(s); }
static pico_result_t do_caps(pico_session_t *s)    { return session_get_caps(s); }
static pico_result_t do_arm(pico_session_t *s)     { return session_arm(s); }
static pico_result_t do_stop(pico_session_t *s)    { return session_stop(s); }

/* ─── Pętla capture / fuzz ──────────────────────────────────────── */
static pico_result_t run_loop(pico_session_t *s)
{
    struct pollfd pfd = { .fd = s->fd, .events = POLLIN };

    printf("[main] Reciever loop\n");
    while (!g_stop) {
        int ret = poll(&pfd, 1, 50);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[main] poll");
            return PICO_ERR_TRANSPORT;
        }
        pico_result_t r = session_pump(s);
        if (r) return r;
    }
    return PICO_OK;
}

int main(int argc, char *argv[])
{
    const char *port = (argc > 1) ? argv[1] : "/dev/ttyACM0";
    int         baud = (argc > 2) ? atoi(argv[2]) : 115200;
    int         mode = 0;   // 0 = capture, 1 = fuzz
    if (argc > 3 && argv[3][0] == 'f') mode = 1;

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    printf("=== pico_host | port=%s baud=%d mode=%s ===\n\n",
           port, baud, mode ? "FUZZ" : "CAPTURE");

    pico_session_t s;

    // 1. Transport
    if (transport_open(&s, port, baud))
        return 1;

    // 2. CSV
    time_t now = time(NULL);
    char csv[64];
    strftime(csv, sizeof(csv), "trace_%Y%m%d_%H%M%S.csv", localtime(&now));
    csv_open(&s, csv);

    // 3. Sekwencja handshake

    /* HELLO → Connected */
    s.session_id = 0x0042;
    if (wait_for_state(&s, do_hello, HW_PROTOCOL_STATE_CONNECTED, 2000)) {
        fprintf(stderr, "[main] HELLO nie powiodło się\n");
        goto cleanup;
    }

    // GET_CAPS → CapabilitiesRead
    if (wait_for_state(&s, do_caps, HW_PROTOCOL_STATE_CAPABILITIES_READ, 2000)) {
        fprintf(stderr, "[main] GET_CAPS nie powiodło się\n");
        goto cleanup;
    }

    // SET_BUS — I2C 400 kHz, GPIO 4 (SDA), GPIO 5 (SCL)
    {
        hw_protocol_set_bus_t bus = {
            .speed_hz      = 400000,
            .bus_type      = HW_PROTOCOL_BUS_I2C,
            .bus_flags     = 0,
            .pin_a         = 4,   // SDA
            .pin_b         = 5,   // SCL
            .uart_parity   = 0,
            .uart_stop_bits= 0,
        };
        if (session_set_bus(&s, &bus)) goto cleanup;
    }

    {
        hw_protocol_set_target_t tgt = {
            .vtarget_mv  = 3300,
            .pin_dir_mask= 0x00,
            .pullup_mode = HW_PROTOCOL_PULLUP_EXTERNAL,
            .pullup_mask = 0x30,   /* GPIO 4 i 5 */
        };
        if (session_set_target(&s, &tgt)) goto cleanup;
    }

    s.state = HW_PROTOCOL_STATE_CONFIGURED;
    printf("[main] State: %s\n", state_name(s.state));

    if (mode == 1) {
        hw_protocol_set_fuzz_policy_t pol = {
            .time_budget_ms = 30000,
            .pending_bytes  = 512,
            .policy_flags   = 0,
            .selection_mode = 0,   // sekwencyjny
            .repeat_mode    = 0,   // raz
            .max_pending    = 8,
        };
        if (session_set_fuzz_policy(&s, &pol)) goto cleanup;

        // Przykładowy
        uint8_t stim[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
        session_queue_stimulus(&s, 1, stim, sizeof(stim),
                               HW_PROTOCOL_STIMULUS_INLINE_PAYLOAD, 0);
    }

    // ARM → Armed
    if (wait_for_state(&s, do_arm, HW_PROTOCOL_STATE_ARMED,
                       HW_PROTOCOL_ARM_TIMEOUT_MS)) {
        fprintf(stderr, "[main] ARM did not work\n");
        goto cleanup;
    }

    // START
    {
        pico_result_t r = (mode == 1) ? session_start_fuzz(&s)
                                      : session_start_capture(&s);
        if (r) goto cleanup;
        s.state = HW_PROTOCOL_STATE_RUNNING;
    }

    run_loop(&s);

    // STOP → Armed
    printf("\n[main] Stopping...\n");
    wait_for_state(&s, do_stop, HW_PROTOCOL_STATE_ARMED,
                   HW_PROTOCOL_STOP_TIMEOUT_MS);

cleanup:
    session_disarm(&s);

    printf("[main] Stats: TX=%llu RX=%llu CRC_err=%llu\n",
           (unsigned long long)s.frames_tx,
           (unsigned long long)s.frames_rx,
           (unsigned long long)s.crc_errors);

    csv_close(&s);
    transport_close(&s);
    return 0;
}
