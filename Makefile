# Makefile for CargoForge-C

CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/cli.c $(SRC_DIR)/parser.c $(SRC_DIR)/analysis.c $(SRC_DIR)/placement_3d.c $(SRC_DIR)/constraints.c $(SRC_DIR)/visualization.c $(SRC_DIR)/json_output.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
HDRS = $(INC_DIR)/cargoforge.h $(INC_DIR)/cli.h $(INC_DIR)/placement_3d.h $(INC_DIR)/constraints.h $(INC_DIR)/visualization.h $(INC_DIR)/json_output.h

TARGET = cargoforge

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_DIR)/test_parser $(TEST_DIR)/test_analysis $(TEST_DIR)/test_constraints

.PHONY: all clean test test-asan test-valgrind

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
	valgrind --leak-check=full --error-exitcode=1 ./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
	@echo "=== Valgrind tests passed ==="

test: $(TEST_DIR)/test_parser $(TEST_DIR)/test_analysis $(TEST_DIR)/test_constraints
	@echo "--- Running All Tests ---"
	./$(TEST_DIR)/test_parser
	./$(TEST_DIR)/test_analysis
	./$(TEST_DIR)/test_constraints
	@echo "-----------------------"

$(TEST_DIR)/test_parser: $(TEST_DIR)/test_parser.c $(HDRS) $(BUILD_DIR)/parser.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_parser.c $(BUILD_DIR)/parser.o -lm

$(TEST_DIR)/test_analysis: $(TEST_DIR)/test_analysis.c $(HDRS) $(BUILD_DIR)/analysis.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_analysis.c $(BUILD_DIR)/analysis.o -lm

$(TEST_DIR)/test_constraints: $(TEST_DIR)/test_constraints.c $(HDRS) $(BUILD_DIR)/constraints.o $(BUILD_DIR)/placement_3d.o
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_constraints.c $(BUILD_DIR)/constraints.o $(BUILD_DIR)/placement_3d.o -lm
