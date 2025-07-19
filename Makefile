# Makefile for CargoForge-C

# Compiler and flags
CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm # -lm for math library if needed in the future

# Source files and object files
SRCS = main.c parser.c optimizer.c analysis.c
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = cargoforge

# Default rule: build the executable
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to compile a .c file into a .o file
%.o: %.c cargoforge.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for cleaning up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
