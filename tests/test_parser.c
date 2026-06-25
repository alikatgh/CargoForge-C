/*
 * tests/test_parser.c - Unit tests for the parser module.
 */
#include <assert.h>    // For the assert() macro
#include <stdio.h>
#include "cargoforge.h"

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


    // Test 3: a cargo parse error must leave ship->cargo NULL, not dangling.
    // A dangling pointer makes ship_cleanup() read/free freed memory (a
    // heap-use-after-free + double-free found by scripts/fuzz.sh). We assert the
    // invariant that prevents it; the fuzzer exercises the full cleanup path under ASan.
    printf("Testing cargo parse-error leaves no dangling pointer...");
    {
        Ship s = {0};
        assert(parse_ship_config("examples/sample_ship.cfg", &s) == 0);
        const char *path = "build/_bad_cargo_test.txt";
        FILE *f = fopen(path, "w");
        assert(f != NULL);
        fputs("GoodItem 10 5x5x5 standard\nBadItem notanumber 5x5x5 reefer\n", f);
        fclose(f);
        assert(parse_cargo_list(path, &s) == -1); // bad weight on line 2
        assert(s.cargo == NULL && s.cargo_count == 0); // freed AND cleared
        remove(path);
    }
    printf(" OK\n");


    printf("--- All Parser Tests Passed ---\n");
    return 0;
}