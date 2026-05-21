#include "pico_host.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static speed_t to_speed(int baud)
{
    switch (baud) {
        case 9600:   return B9600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:
            fprintf(stderr, "[transport] Nieznany baud %d, fallback 115200\n", baud);
            return B115200;
    }
}

pico_result_t transport_open(pico_session_t *s, const char *port, int baud)
{
    memset(s, 0, sizeof(*s));
    s->fd    = -1;
    s->state = HW_PROTOCOL_STATE_DETACHED;
    strncpy(s->port, port, sizeof(s->port) - 1);

    s->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s->fd < 0) {
        perror("[transport] open");
        return PICO_ERR_TRANSPORT;
    }

    struct termios tty;
    if (tcgetattr(s->fd, &tty) != 0) {
        perror("[transport] tcgetattr");
        close(s->fd); s->fd = -1;
        return PICO_ERR_TRANSPORT;
    }

    speed_t sp = to_speed(baud);
    cfsetispeed(&tty, sp);
    cfsetospeed(&tty, sp);
    cfmakeraw(&tty);

    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |=  (CS8 | CREAD | CLOCAL);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(s->fd, TCSANOW, &tty) != 0) {
        perror("[transport] tcsetattr");
        close(s->fd); s->fd = -1;
        return PICO_ERR_TRANSPORT;
    }

    tcflush(s->fd, TCIOFLUSH);
    printf("[transport] Open %s @ %d\n", port, baud);
    return PICO_OK;
}

void transport_close(pico_session_t *s)
{
    if (s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
        printf("[transport] Closed %s\n", s->port);
    }
}

pico_result_t transport_write(pico_session_t *s, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(s->fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[transport] write");
            return PICO_ERR_TRANSPORT;
        }
        sent += (size_t)n;
    }
    return PICO_OK;
}

pico_result_t transport_read(pico_session_t *s)
{
    size_t space = RX_BUF_CAP - s->rx_len;
    if (space == 0) {
        fprintf(stderr, "[transport] rx_buf overflow!\n");
        size_t drop = RX_BUF_CAP / 2;
        memmove(s->rx_buf, s->rx_buf + drop, s->rx_len - drop);
        s->rx_len -= drop;
        return PICO_ERR_OVERFLOW;
    }

    ssize_t n = read(s->fd, s->rx_buf + s->rx_len, space);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return PICO_OK;
        perror("[transport] read");
        return PICO_ERR_TRANSPORT;
    }
    s->rx_len += (size_t)n;
    return PICO_OK;
}
