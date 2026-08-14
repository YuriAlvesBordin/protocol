# Makefile for the hardware-agnostic binary communication protocol example

# Compiler
CC ?= gcc

# Compiler flags
CFLAGS ?= -Iinclude -Wall -Wextra -std=c99

# Target executables
TARGETS = example example_stream

# Source files for example
EXAMPLE_SRCS = src/example.c src/crc16.c src/frame.c src/protocol.c src/stream_rx.c src/stream_tx.c src/datagram.c
# Source files for example_stream
EXAMPLE_STREAM_SRCS = src/example_stream.c src/crc16.c src/frame.c src/protocol.c src/stream_rx.c src/stream_tx.c src/datagram.c

# Object files
EXAMPLE_OBJS = $(EXAMPLE_SRCS:.c=.o)
EXAMPLE_STREAM_OBJS = $(EXAMPLE_STREAM_SRCS:.c=.o)

# Default target
all: $(TARGETS)

# Link the targets
example: $(EXAMPLE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

example_stream: $(EXAMPLE_STREAM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(EXAMPLE_OBJS) $(EXAMPLE_STREAM_OBJS) $(TARGETS)

# Phony targets
.PHONY: all clean