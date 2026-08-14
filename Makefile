# Makefile for the hardware-agnostic binary communication protocol example

# Compiler
CC ?= gcc

# Compiler flags
CFLAGS ?= -Iinclude -Wall -Wextra -std=c99

# Target executable
TARGET = example

# Source files
SRCS = src/example.c src/crc16.c src/frame.c src/protocol.c src/stream_rx.c src/stream_tx.c src/datagram.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean