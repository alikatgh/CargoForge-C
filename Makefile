# Makefile for CargoForge-C
#
# Targets:
#   make            build the optimized release binary (./cargoforge)
#   make debug      build with -g -O0 for a debugger
#   make sanitize   build ./cargoforge-san with AddressSanitizer + UBSan
#   make test       build and run the unit-test suite
#   make run        build and run on the bundled sample scenario
#   make format     run clang-format over all sources (if installed)
#   make clean      remove build artifacts
#
# Pass WERROR=1 to treat warnings as errors (used by CI):  make WERROR=1 test

CC      ?= gcc
STD      = -std=c99 -D_POSIX_C_SOURCE=200809L
WARN     = -Wall -Wextra
CFLAGS  ?= -O3
CFLAGS  += $(STD) $(WARN)
LDFLAGS  = -lm

ifdef WERROR
CFLAGS  += -Werror
endif

# Sanitizer flags for the dedicated `sanitize` target.
SAN_FLAGS = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

# Source files, object files, and header files.
SRCS = main.c parser.c optimizer.c analysis.c placement_2d.c
OBJS = $(SRCS:.c=.o)
HDRS = cargoforge.h placement_2d.h

TARGET = cargoforge

TESTS = tests/test_parser tests/test_placement_2d tests/test_analysis

# Default rule: build the executable.
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile a .c into a .o, rebuilding if any project header changes.
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build (a one-shot compile so it never mixes flags with release objects).
debug:
	$(CC) $(STD) $(WARN) -g -O0 $(SRCS) -o $(TARGET) $(LDFLAGS)

# Sanitized build — catches memory errors and undefined behavior at runtime.
sanitize:
	$(CC) $(STD) $(WARN) $(SAN_FLAGS) $(SRCS) -o $(TARGET)-san $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) examples/sample_ship.cfg examples/sample_cargo.txt

format:
	@command -v clang-format >/dev/null 2>&1 \
		&& clang-format -i $(SRCS) $(HDRS) tests/*.c && echo "Formatted." \
		|| echo "clang-format not installed; skipping."

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)-san $(TESTS)

.PHONY: all debug sanitize run format clean test

# ---- unit-test targets ----
test: $(TESTS)
	@echo "--- Running All Tests ---"
	@for t in $(TESTS); do echo "[$$t]"; ./$$t || exit 1; done
	@echo "--- All tests passed ---"

tests/test_parser: tests/test_parser.c $(HDRS) parser.o
	$(CC) $(CFLAGS) -o $@ tests/test_parser.c parser.o $(LDFLAGS)

tests/test_placement_2d: tests/test_placement_2d.c $(HDRS) placement_2d.o
	$(CC) $(CFLAGS) -o $@ tests/test_placement_2d.c placement_2d.o $(LDFLAGS)

tests/test_analysis: tests/test_analysis.c $(HDRS) analysis.o
	$(CC) $(CFLAGS) -o $@ tests/test_analysis.c analysis.o $(LDFLAGS)
