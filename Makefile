# Makefile for CargoForge-C

CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests

# --- Source file groups ---

# Core engine (no CLI, no main)
LIB_SRCS = $(SRC_DIR)/parser.c $(SRC_DIR)/analysis.c $(SRC_DIR)/placement_3d.c \
           $(SRC_DIR)/constraints.c $(SRC_DIR)/json_output.c \
           $(SRC_DIR)/hydrostatics.c $(SRC_DIR)/tanks.c \
           $(SRC_DIR)/longitudinal_strength.c $(SRC_DIR)/imdg.c \
           $(SRC_DIR)/libcargoforge.c

# CLI + server (needs engine)
CLI_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/cli.c $(SRC_DIR)/visualization.c \
           $(SRC_DIR)/server.c

SRCS = $(CLI_SRCS) $(LIB_SRCS)
LIB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))
CLI_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLI_SRCS))
ALL_OBJS = $(LIB_OBJS) $(CLI_OBJS)

HDRS = $(wildcard $(INC_DIR)/*.h)

TARGET = cargoforge

# Detect platform for shared library extension
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  SHARED_EXT = dylib
  SHARED_FLAGS = -dynamiclib -install_name @rpath/libcargoforge.$(SHARED_EXT)
else
  SHARED_EXT = so
  SHARED_FLAGS = -shared -Wl,-soname,libcargoforge.$(SHARED_EXT)
endif

# --- Targets ---

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(ALL_OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# --- Library targets ---

lib: $(BUILD_DIR) libcargoforge.a libcargoforge.$(SHARED_EXT)

libcargoforge.a: $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

libcargoforge.$(SHARED_EXT): $(LIB_OBJS)
	$(CC) $(SHARED_FLAGS) -o $@ $(LIB_OBJS) $(LDFLAGS)

# --- Install ---

PREFIX ?= /usr/local

install: $(TARGET) libcargoforge.a libcargoforge.$(SHARED_EXT)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	install -m 644 libcargoforge.a $(DESTDIR)$(PREFIX)/lib/
	install -m 755 libcargoforge.$(SHARED_EXT) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(INC_DIR)/libcargoforge.h $(DESTDIR)$(PREFIX)/include/

# --- Clean ---

clean:
	rm -rf $(BUILD_DIR) $(TARGET) libcargoforge.a libcargoforge.$(SHARED_EXT) \
	       $(TEST_DIR)/test_parser $(TEST_DIR)/test_analysis \
	       $(TEST_DIR)/test_constraints $(TEST_DIR)/test_hydrostatics $(TEST_DIR)/test_tanks \
	       $(TEST_DIR)/test_longitudinal_strength $(TEST_DIR)/test_imdg \
	       $(TEST_DIR)/test_library examples/library_example

.PHONY: all lib clean install test test-asan test-valgrind wasm example

# --- Sanitizer builds ---

test-asan: CFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
test-asan: LDFLAGS += -fsanitize=address -fsanitize=undefined
test-asan: clean all test
	@echo "=== Memory safety tests (ASAN/UBSAN) passed ==="

test-valgrind: all test
	@echo "=== Running tests with Valgrind ==="
	@command -v valgrind >/dev/null 2>&1 || { echo "Valgrind not installed"; exit 1; }
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_parser
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_analysis
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_constraints
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_hydrostatics
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_tanks
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_longitudinal_strength
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_imdg
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_library
	valgrind --leak-check=full --error-exitcode=1 ./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
	@echo "=== Valgrind tests passed ==="

# --- Tests ---

test: $(TEST_DIR)/test_parser $(TEST_DIR)/test_analysis $(TEST_DIR)/test_constraints \
      $(TEST_DIR)/test_hydrostatics $(TEST_DIR)/test_tanks \
      $(TEST_DIR)/test_longitudinal_strength $(TEST_DIR)/test_imdg \
      $(TEST_DIR)/test_library
	@echo "--- Running All Tests ---"
	./$(TEST_DIR)/test_parser
	./$(TEST_DIR)/test_analysis
	./$(TEST_DIR)/test_constraints
	./$(TEST_DIR)/test_hydrostatics
	./$(TEST_DIR)/test_tanks
	./$(TEST_DIR)/test_longitudinal_strength
	./$(TEST_DIR)/test_imdg
	./$(TEST_DIR)/test_library
	@echo "-----------------------"

$(TEST_DIR)/test_parser: $(TEST_DIR)/test_parser.c $(HDRS) $(BUILD_DIR)/parser.o $(BUILD_DIR)/hydrostatics.o $(BUILD_DIR)/tanks.o $(BUILD_DIR)/longitudinal_strength.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_parser.c $(BUILD_DIR)/parser.o $(BUILD_DIR)/hydrostatics.o $(BUILD_DIR)/tanks.o $(BUILD_DIR)/longitudinal_strength.o -lm

$(TEST_DIR)/test_analysis: $(TEST_DIR)/test_analysis.c $(HDRS) $(BUILD_DIR)/analysis.o $(BUILD_DIR)/hydrostatics.o $(BUILD_DIR)/tanks.o $(BUILD_DIR)/longitudinal_strength.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_analysis.c $(BUILD_DIR)/analysis.o $(BUILD_DIR)/hydrostatics.o $(BUILD_DIR)/tanks.o $(BUILD_DIR)/longitudinal_strength.o -lm

$(TEST_DIR)/test_constraints: $(TEST_DIR)/test_constraints.c $(HDRS) $(BUILD_DIR)/constraints.o $(BUILD_DIR)/placement_3d.o $(BUILD_DIR)/imdg.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_constraints.c $(BUILD_DIR)/constraints.o $(BUILD_DIR)/placement_3d.o $(BUILD_DIR)/imdg.o -lm

$(TEST_DIR)/test_hydrostatics: $(TEST_DIR)/test_hydrostatics.c $(HDRS) $(BUILD_DIR)/hydrostatics.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_hydrostatics.c $(BUILD_DIR)/hydrostatics.o -lm

$(TEST_DIR)/test_tanks: $(TEST_DIR)/test_tanks.c $(HDRS) $(BUILD_DIR)/tanks.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_tanks.c $(BUILD_DIR)/tanks.o -lm

$(TEST_DIR)/test_longitudinal_strength: $(TEST_DIR)/test_longitudinal_strength.c $(HDRS) $(BUILD_DIR)/longitudinal_strength.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_longitudinal_strength.c $(BUILD_DIR)/longitudinal_strength.o -lm

$(TEST_DIR)/test_imdg: $(TEST_DIR)/test_imdg.c $(HDRS) $(BUILD_DIR)/imdg.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_imdg.c $(BUILD_DIR)/imdg.o -lm

$(TEST_DIR)/test_library: $(TEST_DIR)/test_library.c $(HDRS) libcargoforge.a
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_library.c libcargoforge.a $(LDFLAGS)

# --- Example ---

example: libcargoforge.a examples/library_example
examples/library_example: examples/library_example.c $(HDRS) libcargoforge.a
	$(CC) $(CFLAGS) -o $@ examples/library_example.c libcargoforge.a $(LDFLAGS)

# --- WASM (requires Emscripten) ---

wasm: $(HDRS)
	@command -v emcc >/dev/null 2>&1 || { echo "Emscripten (emcc) not found. Install: https://emscripten.org"; exit 1; }
	mkdir -p wasm
	emcc -O3 -std=c99 -Iinclude \
		-s EXPORTED_FUNCTIONS='["_cargoforge_open","_cargoforge_close","_cargoforge_load_ship_string","_cargoforge_load_cargo_string","_cargoforge_optimize","_cargoforge_result_json","_cargoforge_version","_malloc","_free"]' \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME=CargoForge \
		-o wasm/cargoforge.js \
		$(LIB_SRCS)
	@echo "=== WASM build complete: wasm/cargoforge.js + wasm/cargoforge.wasm ==="
