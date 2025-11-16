# Makefile for 2-Board Walkie-Talkie

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -lpthread -lopus

TARGET = walkietalkie

SRCS = walkietalkie.c \
       opus_helper.c \
       network.c \
       audio_dma.c \
       gpio_ptt.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

install: $(TARGET)
	install -m 0755 $(TARGET) $(DESTDIR)/usr/bin/

.PHONY: all clean install