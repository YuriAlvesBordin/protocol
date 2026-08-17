# Makefile for the hardware-agnostic binary communication protocol example

# Compiler
CC ?= gcc

# Compiler flags
CFLAGS ?= -Iinclude -Wall -Wextra -std=c99

# Target executables
TARGETS = datagram_example streaming_example

# Source files for datagram example
DATAGRAM_SRCS = src/datagram_example.c src/crc16.c src/frame.c src/protocol.c src/stream_rx.c src/stream_tx.c src/datagram.c
# Source files for streaming example
STREAMING_SRCS = src/streaming_example.c src/crc16.c src/frame.c src/protocol.c src/stream_rx.c src/stream_tx.c src/datagram.c

# Object files
DATAGRAM_OBJS = $(DATAGRAM_SRCS:.c=.o)
STREAMING_OBJS = $(STREAMING_SRCS:.c=.o)

# Default target
all: $(TARGETS)

# Link the targets
datagram_example: $(DATAGRAM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

streaming_example: $(STREAMING_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(DATAGRAM_OBJS) $(STREAMING_OBJS) $(TARGETS)

# Phony targets
.PHONY: all clean