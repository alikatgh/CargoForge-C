# Makefile for CargoForge-C

# Compiler and flags
CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm # -lm for math library if needed in the future

# Source files, object files, and header files
SRCS = main.c parser.c optimizer.c analysis.c placement_2d.c placement_3d.c constraints.c visualization.c
OBJS = $(SRCS:.c=.o)
HDRS = cargoforge.h placement_2d.h placement_3d.h constraints.h visualization.h

# Executable name
TARGET = cargoforge

# Default rule: build the executable
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to compile a .c file into a .o file
# It now depends on all project headers for correctness.
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Makefile for CargoForge-C (with new test target)

# ... (previous content is the same) ...

# Rule for cleaning up build files
clean:
	rm -f $(OBJS) $(TARGET) tests/test_parser tests/test_placement_2d tests/test_analysis

# Phony targets
.PHONY: all clean test test-asan test-valgrind

# Memory safety testing
test-asan: CFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
test-asan: LDFLAGS += -fsanitize=address -fsanitize=undefined
test-asan: clean all test
	@echo "=== Memory safety tests (ASAN/UBSAN) passed ==="

test-valgrind: all test
	@echo "=== Running tests with Valgrind ==="
	@command -v valgrind >/dev/null 2>&1 || { echo "Valgrind not installed"; exit 1; }
	valgrind --leak-check=full --error-exitcode=1 ./tests/test_parser
	valgrind --leak-check=full --error-exitcode=1 ./tests/test_placement_2d
	valgrind --leak-check=full --error-exitcode=1 ./tests/test_analysis
	valgrind --leak-check=full --error-exitcode=1 ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
	@echo "=== Valgrind tests passed ==="

# ---- unit-test targets ----
# Make 'test' a phony target that runs all individual tests
test: tests/test_parser tests/test_placement_2d tests/test_analysis
	@echo "--- Running All Tests ---"
	./tests/test_parser
	./tests/test_placement_2d
	./tests/test_analysis
	@echo "-----------------------"

tests/test_parser: tests/test_parser.c cargoforge.h parser.o
	$(CC) $(CFLAGS) -o $@ tests/test_parser.c parser.o -lm

tests/test_placement_2d: tests/test_placement_2d.c placement_2d.o
	$(CC) $(CFLAGS) -o $@ $^

tests/test_analysis: tests/test_analysis.c analysis.o
	$(CC) $(CFLAGS) -o $@ $^ -lm