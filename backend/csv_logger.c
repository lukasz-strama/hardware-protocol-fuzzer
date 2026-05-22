#include "pico_host.h"

int csv_open(pico_session_t *s, const char *path)
{
    strncpy(s->csv_path, path, sizeof(s->csv_path) - 1);
    s->csv_fp = fopen(path, "a");
    if (!s->csv_fp) { perror("[csv] fopen"); return -1; }

    // Nagłówek tylko gdy plik jest nowy
    fseek(s->csv_fp, 0, SEEK_END);
    if (ftell(s->csv_fp) == 0)
        fprintf(s->csv_fp,
            "timestamp_iso,trace_seq,timestamp_us,bus,event_type,"
            "data_len,data_hex\n");

    fflush(s->csv_fp);
    printf("[csv] Otwarty: %s\n", path);
    return 0;
}

void csv_log_trace(pico_session_t *s,
                   const hw_protocol_frame_header_t *hdr,
                   const hw_protocol_trace_decoded_t *tr)
{
    if (!s->csv_fp) return;
    (void)hdr;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    char iso[32];
    struct tm *tm = localtime(&now.tv_sec);
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", tm);

    static const char *ev[] = {"BYTE","START","STOP","ACK","NACK","BREAK","OVERFLOW"};
    const char *ev_name = (tr->event_type < 7) ? ev[tr->event_type] : "UNK";

    char hex[HW_PROTOCOL_MAX_TRACE_CHUNK * 2 + 1] = {0};
    const uint8_t *data = tr->data;
    for (uint16_t i = 0; i < tr->data_len && i < HW_PROTOCOL_MAX_TRACE_CHUNK; i++)
        snprintf(hex + i * 2, 3, "%02X", data[i]);

    fprintf(s->csv_fp, "%s.%03ld,%u,%u,%s,%s,%u,%s\n",
            iso, now.tv_nsec / 1000000,
            tr->trace_seq, tr->timestamp_us,
            tr->source_bus ? "UART" : "I2C",
            ev_name,
            tr->data_len,
            hex);

    s->csv_rows++;

    // Flush co 100 wierszy
    if (s->csv_rows % 100 == 0)
        fflush(s->csv_fp);
}

void csv_close(pico_session_t *s)
{
    if (s->csv_fp) {
        fflush(s->csv_fp);
        fclose(s->csv_fp);
        s->csv_fp = NULL;
        printf("[csv] Closed %s (%llu rows)\n",
               s->csv_path, (unsigned long long)s->csv_rows);
    }
}
