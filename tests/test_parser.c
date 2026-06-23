/*
 * tests/test_parser.c - Unit tests for the parser module.
 *
 * Run from the project root (paths are relative to it, as in `make test`).
 */
#include <assert.h>
#include <math.h>     /* for the sentinel checks */
#include <stdio.h>
#include <stdlib.h>   /* free */
#include <string.h>   /* strcmp */

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

static void test_inline_comments_and_whitespace(void) {
    printf("  config tolerates inline comments + whitespace... ");
    Ship ship = {0};
    assert(parse_ship_config("tests/fixtures/inline_comment_ship.cfg", &ship) == 0);
    assert(ship.length == 120.0f);
    assert(ship.width == 18.0f);
    assert(ship.max_weight == 9000.0f * 1000.0f);
    assert(ship.hold_count == 3);
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

static void test_parses_cargo_attributes(void) {
    printf("  parse optional cargo attributes (5th field)... ");
    Ship ship = {0};
    assert(parse_cargo_list("tests/fixtures/attrs_cargo.txt", &ship) == 0);
    assert(ship.cargo_count == 4);
    /* Reefer1 */
    assert(ship.cargo[0].reefer && !ship.cargo[0].fragile && ship.cargo[0].stackable);
    /* Hazmat1: dg=3, priority */
    assert(ship.cargo[1].dg_class == 3 && ship.cargo[1].priority);
    /* Glass: fragile implies not stackable */
    assert(ship.cargo[2].fragile && !ship.cargo[2].stackable);
    /* Plain: defaults — stackable, no flags, no DG */
    assert(ship.cargo[3].stackable && !ship.cargo[3].priority && ship.cargo[3].dg_class == 0);
    free(ship.cargo);
    printf("OK\n");
}

static void test_rejects_empty_cargo_list(void) {
    printf("  reject empty cargo list... ");
    Ship ship = {0};
    assert(parse_cargo_list("tests/fixtures/empty_cargo.txt", &ship) == -1);
    printf("OK\n");
}

static void test_parses_optional_holds_field(void) {
    printf("  parse optional holds field (valid + out-of-range)... ");
    Ship ship = {0};
    assert(parse_ship_config("tests/fixtures/holds4_ship.cfg", &ship) == 0);
    assert(ship.hold_count == 4);

    Ship bad = {0};
    assert(parse_ship_config("tests/fixtures/holds_bad_ship.cfg", &bad) == -1);
    printf("OK\n");
}

static void test_duplicate_ids_warn_but_parse(void) {
    printf("  duplicate IDs are warned, not rejected... ");
    Ship ship = {0};
    /* Duplicates are a warning (printed to stderr), not a parse failure. */
    assert(parse_cargo_list("tests/fixtures/dup_cargo.txt", &ship) == 0);
    assert(ship.cargo_count == 3);
    free(ship.cargo);
    printf("OK\n");
}

static void test_skips_over_long_line(void) {
    printf("  skip over-long line, parse the rest... ");
    Ship ship = {0};
    /* Fixture: GoodA, a >255-char pathological line, GoodB. The long line must be
     * skipped (not misparsed as a phantom second line), leaving exactly 2 items. */
    assert(parse_cargo_list("tests/fixtures/longline_cargo.txt", &ship) == 0);
    assert(ship.cargo_count == 2);
    assert(strcmp(ship.cargo[0].id, "GoodA") == 0);
    assert(strcmp(ship.cargo[1].id, "GoodB") == 0);
    free(ship.cargo);
    printf("OK\n");
}

static void test_parses_csv_cargo(void) {
    printf("  parse CSV cargo manifest (header skipped)... ");
    Ship ship = {0};
    assert(parse_cargo_list("tests/fixtures/cargo.csv", &ship) == 0);
    assert(ship.cargo_count == 4); // 4 data rows, header excluded
    assert(strcmp(ship.cargo[0].id, "HeavyMachinery") == 0);
    assert(ship.cargo[0].weight == 550.0f * 1000.0f);
    free(ship.cargo);
    printf("OK\n");
}

static void test_parses_logistics_attributes(void) {
    printf("  parse temp/maxstack/dest attributes... ");
    Ship ship = {0};
    assert(parse_cargo_list("tests/fixtures/logistics_cargo.txt", &ship) == 0);
    assert(ship.cargo_count == 5);
    /* ReeferA: temp=-18, dest=ROTTERDAM, reefer set by temp= */
    assert(ship.cargo[0].temp_c == -18.0f);
    assert(ship.cargo[0].reefer);
    assert(strcmp(ship.cargo[0].dest, "ROTTERDAM") == 0);
    /* HeavyBase: maxstack=20 */
    assert(ship.cargo[3].max_stack_t == 20.0f);
    /* GeneralD: no temp set -> NAN */
    assert(isnan(ship.cargo[4].temp_c));
    free(ship.cargo);
    printf("OK\n");
}

static void test_parses_json_config(void) {
    printf("  parse JSON ship config... ");
    Ship ship = {0};
    assert(parse_ship_config("tests/fixtures/ship.json", &ship) == 0);
    assert(ship.length == 150.0f);
    assert(ship.width == 25.0f);
    assert(ship.depth == 14.0f);
    assert(ship.max_weight == 50000.0f * 1000.0f);
    printf("OK\n");
}

int main(void) {
    printf("--- Running Parser Tests ---\n");
    test_rejects_non_numeric_field();
    test_accepts_valid_config_and_parses_values();
    test_inline_comments_and_whitespace();
    test_rejects_missing_required_field();
    test_parses_cargo_and_initializes_sentinels();
    test_rejects_empty_cargo_list();
    test_parses_optional_holds_field();
    test_duplicate_ids_warn_but_parse();
    test_parses_cargo_attributes();
    test_skips_over_long_line();
    test_parses_csv_cargo();
    test_parses_json_config();
    test_parses_logistics_attributes();
    printf("--- All Parser Tests Passed ---\n");
    return 0;
}
