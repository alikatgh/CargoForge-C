/*
 * test_constraints.c - Unit tests for constraints module
 *
 * Tests hazmat separation, point load, stacking pressure, and deck weight.
 */

#include "cargoforge.h"
#include "constraints.h"
#include "placement_3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static Ship create_test_ship(void) {
    Ship ship = {
        .length = 100.0f,
        .width = 20.0f,
        .max_weight = 10000000.0f,
        .lightship_weight = 2000000.0f,
        .lightship_kg = 5.0f,
        .cargo_count = 0,
        .cargo_capacity = 10,
        .cargo = NULL
    };
    ship.cargo = malloc(ship.cargo_capacity * sizeof(Cargo));
    return ship;
}

static Bin3D create_test_bin(const char *name, float max_wt) {
    Bin3D bin;
    memset(&bin, 0, sizeof(bin));
    strncpy(bin.name, name, sizeof(bin.name) - 1);
    bin.max_weight = max_wt;
    bin.current_weight = 0;
    bin.spaces[0] = (Space3D){0, 0, 0, 100, 20, 8, 1};
    bin.space_count = 1;
    return bin;
}

/* Test 1: Hazmat type detection */
void test_cargo_type_detection(void) {
    printf("Test 1: Cargo type detection... ");

    Cargo haz = {.type = "hazardous"};
    Cargo fra = {.type = "fragile"};
    Cargo ref = {.type = "reefer"};
    Cargo std = {.type = "standard"};

    assert(is_hazardous(&haz) == 1);
    assert(is_hazardous(&std) == 0);
    assert(is_fragile(&fra) == 1);
    assert(is_fragile(&std) == 0);
    assert(is_reefer(&ref) == 1);
    assert(is_reefer(&std) == 0);

    printf("PASS\n");
}

/* Test 2: Point load calculation */
void test_point_load(void) {
    printf("Test 2: Point load calculation... ");

    Cargo c = {.weight = 20000.0f, .dimensions = {4.0f, 5.0f, 2.0f}};
    float pl = calculate_point_load(&c);
    /* 20t / (4*5) = 1.0 t/m2 */
    assert(fabsf(pl - 1.0f) < 0.01f);

    /* Tiny cargo - no division by zero */
    Cargo tiny = {.weight = 100.0f, .dimensions = {0.001f, 0.001f, 1.0f}};
    float pl2 = calculate_point_load(&tiny);
    assert(pl2 == 0.0f);

    printf("PASS\n");
}

/* Test 3: Hazmat separation - too close */
void test_hazmat_too_close(void) {
    printf("Test 3: Hazmat too close... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "Haz1", .weight = 5000.0f,
        .dimensions = {2.0f, 2.0f, 2.0f}, .type = "hazardous",
        .pos_x = 10.0f, .pos_y = 10.0f, .pos_z = 0.0f
    };

    Cargo new_haz = {
        .id = "Haz2", .weight = 5000.0f,
        .dimensions = {2.0f, 2.0f, 2.0f}, .type = "hazardous"
    };

    /* 1m away - should fail (min 3m) */
    assert(check_hazmat_separation(&ship, &new_haz, 11.0f, 10.0f, 0.0f) == 0);

    /* 5m away - should pass */
    assert(check_hazmat_separation(&ship, &new_haz, 15.0f, 10.0f, 0.0f) == 1);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 4: Hazmat separation - non-hazmat always passes */
void test_hazmat_non_hazardous(void) {
    printf("Test 4: Non-hazmat always passes separation... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "Haz1", .weight = 5000.0f,
        .dimensions = {2.0f, 2.0f, 2.0f}, .type = "hazardous",
        .pos_x = 10.0f, .pos_y = 10.0f, .pos_z = 0.0f
    };

    Cargo standard = {
        .id = "Std1", .weight = 5000.0f,
        .dimensions = {2.0f, 2.0f, 2.0f}, .type = "standard"
    };

    /* Standard cargo right next to hazmat - should pass */
    assert(check_hazmat_separation(&ship, &standard, 10.5f, 10.0f, 0.0f) == 1);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 5: Stacking pressure - no cargo above */
void test_stack_pressure_empty(void) {
    printf("Test 5: Stack pressure with nothing above... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 0;

    float p = calculate_stack_pressure(&ship, 10.0f, 5.0f, 0.0f, 4.0f, 3.0f);
    assert(p == 0.0f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 6: Stacking pressure - cargo above */
void test_stack_pressure_with_load(void) {
    printf("Test 6: Stack pressure with cargo above... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    /* 10t cargo directly above (full overlap) */
    ship.cargo[0] = (Cargo){
        .id = "Top", .weight = 10000.0f,  /* 10t */
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 10.0f, .pos_y = 5.0f, .pos_z = 2.0f  /* above z=0 */
    };

    /* Footprint at z=0, same XY as cargo above */
    float p = calculate_stack_pressure(&ship, 10.0f, 5.0f, 0.0f, 4.0f, 3.0f);
    /* 10t / (4*3) = 0.833 t/m2 */
    assert(p > 0.8f && p < 0.9f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 7: Stacking pressure - partial overlap */
void test_stack_pressure_partial_overlap(void) {
    printf("Test 7: Stack pressure partial overlap... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    /* 12t cargo at (10,5,2), dims 4x3 */
    ship.cargo[0] = (Cargo){
        .id = "Top", .weight = 12000.0f,  /* 12t */
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 10.0f, .pos_y = 5.0f, .pos_z = 2.0f
    };

    /* Half overlap: query at (12,5,0), width=4, depth=3 */
    /* Overlap: x=[12,14] (2m), y=[5,8] (3m) = 6m2 */
    /* Cargo area = 4*3=12m2, overlap fraction = 6/12 = 0.5 */
    /* Weight contribution = 12t * 0.5 = 6t */
    /* Pressure = 6t / (4*3) = 0.5 t/m2 */
    float p = calculate_stack_pressure(&ship, 12.0f, 5.0f, 0.0f, 4.0f, 3.0f);
    assert(p > 0.45f && p < 0.55f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 8: Deck weight constraint */
void test_deck_weight_limit(void) {
    printf("Test 8: Deck weight constraint... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 0;

    Bin3D deck = create_test_bin("Deck", ship.max_weight * 0.4f);
    /* Already 29% loaded */
    deck.current_weight = ship.max_weight * 0.29f;
    Space3D space = {0, 0, 0, 100, 20, 4, 1};

    /* Adding 2% more - total 31% > 30% limit */
    Cargo heavy = {
        .id = "DeckH", .weight = ship.max_weight * 0.02f,
        .dimensions = {5.0f, 5.0f, 2.0f}, .type = "standard"
    };

    int ok = check_cargo_constraints(&ship, &heavy, &deck, &space);
    assert(ok == 0);  /* Should be rejected */

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 9: Standard cargo passes all constraints */
void test_standard_cargo_passes(void) {
    printf("Test 9: Standard cargo passes all constraints... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 0;

    Bin3D hold = create_test_bin("ForwardHold", ship.max_weight * 0.3f);
    Space3D space = {10, 5, -8, 10, 10, 8, 1};

    Cargo normal = {
        .id = "Normal", .weight = 50000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard"
    };

    int ok = check_cargo_constraints(&ship, &normal, &hold, &space);
    assert(ok == 1);

    free(ship.cargo);
    printf("PASS\n");
}

int main(void) {
    printf("\n=== Running Constraints Module Tests ===\n\n");

    test_cargo_type_detection();
    test_point_load();
    test_hazmat_too_close();
    test_hazmat_non_hazardous();
    test_stack_pressure_empty();
    test_stack_pressure_with_load();
    test_stack_pressure_partial_overlap();
    test_deck_weight_limit();
    test_standard_cargo_passes();

    printf("\n=== All Constraints Tests Passed! ===\n\n");
    return 0;
}
