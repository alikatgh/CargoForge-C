/*
 * tests/test_parser.c - Unit tests for the parser module.
 *
 * Run from the project root (paths are relative to it, as in `make test`).
 */
#include <assert.h>
#include <math.h>     /* for the sentinel checks */
#include <stdio.h>
#include <stdlib.h>   /* free */

#include "../cargoforge.h"

static void test_rejects_non_numeric_field(void) {
    printf("  reject bad_ship.cfg (non-numeric length)... ");
    Ship ship = {0};
    assert(parse_ship_config("examples/bad_ship.cfg", &ship) == -1);
    printf("OK\n");
}

static void test_accepts_valid_config_and_parses_values(void) {
    printf("  accept sample_ship.cfg and read values... ");
    Ship ship = {0};
    assert(parse_ship_config("examples/sample_ship.cfg", &ship) == 0);
    /* length_m=150, width_m=25, max_weight_tonnes=50000 (stored in kg). */
    assert(ship.length == 150.0f);
    assert(ship.width == 25.0f);
    assert(ship.max_weight == 50000.0f * 1000.0f);
    printf("OK\n");
}

static void test_rejects_missing_required_field(void) {
    printf("  reject config missing width_m... ");
    Ship ship = {0};
    assert(parse_ship_config("tests/fixtures/missing_width.cfg", &ship) == -1);
    printf("OK\n");
}

static void test_parses_cargo_and_initializes_sentinels(void) {
    printf("  parse cargo list, positions sentinel-initialized... ");
    Ship ship = {0};
    assert(parse_cargo_list("examples/sample_cargo.txt", &ship) == 0);
    assert(ship.cargo_count == 5);
    assert(ship.cargo != NULL);

    /* Every freshly parsed item must have its position sentinel set to < 0, not
     * left as heap garbage (the uninitialized-memory bug this guards against). */
    for (int i = 0; i < ship.cargo_count; i++) {
        assert(ship.cargo[i].pos_x < 0.0f);
        assert(ship.cargo[i].pos_y < 0.0f);
        assert(ship.cargo[i].pos_z < 0.0f);
    }
    /* First entry: HeavyMachinery 550t -> 550000 kg. */
    assert(ship.cargo[0].weight == 550.0f * 1000.0f);

    free(ship.cargo);
    printf("OK\n");
}

static void test_rejects_empty_cargo_list(void) {
    printf("  reject empty cargo list... ");
    Ship ship = {0};
    assert(parse_cargo_list("tests/fixtures/empty_cargo.txt", &ship) == -1);
    printf("OK\n");
}

int main(void) {
    printf("--- Running Parser Tests ---\n");
    test_rejects_non_numeric_field();
    test_accepts_valid_config_and_parses_values();
    test_rejects_missing_required_field();
    test_parses_cargo_and_initializes_sentinels();
    test_rejects_empty_cargo_list();
    printf("--- All Parser Tests Passed ---\n");
    return 0;
}
