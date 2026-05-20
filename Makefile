CC     = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude -g -O2
SRCS   = src/transport.c src/frame.c src/session.c \
         src/csv_logger.c src/diag.c src/main.c
TARGET = pico_host

$(TARGET): $(SRCS) include/pico_host.h include/protocol_layout.h
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f $(TARGET) *.csv
