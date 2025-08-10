/*
 * tests/test_parser.c - Unit tests for the parser module.
 */
#include <assert.h>    // For the assert() macro
#include <stdio.h>
#include "../cargoforge.h" // Use .. to go up one directory to find the header

int main() {
    printf("--- Running Parser Tests ---\n");
    Ship test_ship = {0};

    // Test 1: Ensure the parser correctly rejects a file with bad data.
    // The parse_ship_config() function should return -1 on error.
    printf("Testing rejection of bad_ship.cfg...");
    int result = parse_ship_config("examples/bad_ship.cfg", &test_ship);
    
    // assert() will crash the program if the condition is false.
    // This is how we automatically verify the test.
    assert(result == -1);
    
    printf(" OK\n");


    // Test 2: Ensure the parser correctly accepts a good file.
    // The function should return 0 on success.
    printf("Testing acceptance of sample_ship.cfg...");
    result = parse_ship_config("examples/sample_ship.cfg", &test_ship);
    assert(result == 0);
    printf(" OK\n");


    printf("--- All Parser Tests Passed ---\n");
    return 0;
}