CC     = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -I. -g -O2
SRCS   = transport.c frame.c session.c \
         csv_logger.c diag.c main.c
TARGET = pico_host

$(TARGET): $(SRCS) pico_host.h protocol_layout.h
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f $(TARGET) *.csv
