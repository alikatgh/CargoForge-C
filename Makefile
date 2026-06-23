# Makefile for CargoForge-C
#
# Targets:
#   make            build the optimized release binary (./cargoforge)
#   make debug      build with -g -O0 for a debugger
#   make sanitize   build ./cargoforge-san with AddressSanitizer + UBSan
#   make analyze    run the Clang static analyzer over the sources
#   make test       build and run the unit-test suite
#   make install    install the binary + man page (PREFIX=/usr/local, DESTDIR-aware)
#   make lib        build libcargoforge.a (engine as a static library)
#   make bench      throughput benchmark on a synthetic large manifest
#   make coverage   line-coverage report via gcov (best-effort)
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

# Engine object files (everything except the CLI entry point) for the library.
LIB     = libcargoforge.a
LIBOBJS = parser.o optimizer.o analysis.o placement_2d.o

# Install locations (override with `make PREFIX=/opt install`; honours DESTDIR).
PREFIX ?= /usr/local
BINDIR  = $(DESTDIR)$(PREFIX)/bin
MANDIR  = $(DESTDIR)$(PREFIX)/share/man/man1

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

# Static analysis via the Clang static analyzer (no extra tooling needed).
analyze:
	clang --analyze -Xclang -analyzer-output=text $(STD) $(WARN) $(SRCS)
	@echo "Static analysis clean."

# Build the engine as a static library; consumers link it and include cargoforge.h.
lib: $(LIBOBJS)
	ar rcs $(LIB) $(LIBOBJS)
	@echo "Built $(LIB) — link with it and #include \"cargoforge.h\"."

# Throughput benchmark on a synthetic large manifest.
bench: $(TARGET)
	@./scripts/bench.sh

# Best-effort line coverage via gcov (instruments a one-shot build, exercises the
# CLI across scenarios, then summarizes). Needs a --coverage-capable compiler + gcov.
COVOBJS = $(addprefix $(TARGET)-cov-, $(SRCS:.c=))
coverage:
	$(CC) --coverage -O0 $(STD) $(SRCS) -o $(TARGET)-cov $(LDFLAGS)
	-./$(TARGET)-cov examples/sample_ship.cfg examples/sample_cargo.txt >/dev/null 2>&1
	-./$(TARGET)-cov examples/realistic_ship.cfg examples/realistic_cargo.txt >/dev/null 2>&1
	-./$(TARGET)-cov --json examples/realistic_ship.cfg examples/realistic_cargo.txt >/dev/null 2>&1
	-./$(TARGET)-cov --strict --verbose --color=always examples/sample_ship.cfg examples/sample_cargo.txt >/dev/null 2>&1
	-./$(TARGET)-cov --help >/dev/null 2>&1
	@echo "Line coverage:"
	@gcov $(COVOBJS) 2>/dev/null | grep -A1 -E "^File '(main|parser|optimizer|analysis|placement_2d)\.c'" | grep -vE "^--" || echo "gcov unavailable; .gcda files written."

run: $(TARGET)
	./$(TARGET) examples/sample_ship.cfg examples/sample_cargo.txt

format:
	@command -v clang-format >/dev/null 2>&1 \
		&& clang-format -i $(SRCS) $(HDRS) tests/*.c && echo "Formatted." \
		|| echo "clang-format not installed; skipping."

# Install the binary and man page (PREFIX/DESTDIR aware).
install: $(TARGET)
	install -d $(BINDIR) $(MANDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	install -m 644 docs/$(TARGET).1 $(MANDIR)/$(TARGET).1

uninstall:
	rm -f $(BINDIR)/$(TARGET) $(MANDIR)/$(TARGET).1

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)-san $(TARGET)-cov $(TESTS) $(LIB) *.plist *.gcno *.gcda *.gcov

.PHONY: all debug sanitize analyze lib bench coverage run format install uninstall clean test

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
