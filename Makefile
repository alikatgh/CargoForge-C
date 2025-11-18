# Makefile for CargoForge-C

# Compiler and flags
CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS = -lm # -lm for math library if needed in the future

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests

# Source files, object files, and header files
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/cli.c $(SRC_DIR)/parser.c $(SRC_DIR)/optimizer.c $(SRC_DIR)/analysis.c $(SRC_DIR)/placement_2d.c $(SRC_DIR)/placement_3d.c $(SRC_DIR)/constraints.c $(SRC_DIR)/visualization.c $(SRC_DIR)/json_output.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
HDRS = $(INC_DIR)/cargoforge.h $(INC_DIR)/cli.h $(INC_DIR)/placement_2d.h $(INC_DIR)/placement_3d.h $(INC_DIR)/constraints.h $(INC_DIR)/visualization.h $(INC_DIR)/json_output.h

# Executable name
TARGET = cargoforge

# Default rule: build the executable
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to compile a .c file into a .o file
# It now depends on all project headers for correctness.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Makefile for CargoForge-C (with new test target)

# ... (previous content is the same) ...

# Rule for cleaning up build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_DIR)/test_parser $(TEST_DIR)/test_placement_2d $(TEST_DIR)/test_analysis

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
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_parser
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_placement_2d
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_DIR)/test_analysis
	valgrind --leak-check=full --error-exitcode=1 ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
	@echo "=== Valgrind tests passed ==="

# ---- unit-test targets ----
# Make 'test' a phony target that runs all individual tests
test: $(TEST_DIR)/test_parser $(TEST_DIR)/test_placement_2d $(TEST_DIR)/test_analysis
	@echo "--- Running All Tests ---"
	./$(TEST_DIR)/test_parser
	./$(TEST_DIR)/test_placement_2d
	./$(TEST_DIR)/test_analysis
	@echo "-----------------------"

$(TEST_DIR)/test_parser: $(TEST_DIR)/test_parser.c $(HDRS) $(BUILD_DIR)/parser.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_parser.c $(BUILD_DIR)/parser.o -lm

$(TEST_DIR)/test_placement_2d: $(TEST_DIR)/test_placement_2d.c $(HDRS) $(BUILD_DIR)/placement_2d.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_placement_2d.c $(BUILD_DIR)/placement_2d.o

$(TEST_DIR)/test_analysis: $(TEST_DIR)/test_analysis.c $(HDRS) $(BUILD_DIR)/analysis.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_analysis.c $(BUILD_DIR)/analysis.o -lm