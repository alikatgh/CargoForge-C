/*
 * test_analysis.c - Unit tests for analysis module
 *
 * Tests hydrostatics, trim/heel, CG calculations, and IMO criteria.
 */

#include "cargoforge.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

static Ship create_test_ship(void) {
    Ship ship = {
        .length = 100.0f,
        .width = 20.0f,
        .max_weight = 10000000.0f,   /* 10,000 t */
        .lightship_weight = 2000000.0f, /* 2,000 t */
        .lightship_kg = 5.0f,
        .cargo_count = 0,
        .cargo_capacity = 10,
        .cargo = NULL
    };
    ship.cargo = malloc(ship.cargo_capacity * sizeof(Cargo));
    return ship;
}

/* Test 1: Empty ship - valid GM, zero trim/heel */
void test_empty_ship(void) {
    printf("Test 1: Empty ship analysis... ");
    Ship ship = create_test_ship();
    AnalysisResult r = perform_analysis(&ship);

    assert(r.placed_item_count == 0);
    assert(r.total_cargo_weight_kg == 0.0f);
    assert(!isnan(r.gm));
    assert(r.gm > 0.0f);
    assert(r.draft > 0.0f);
    assert(r.kb > 0.0f);
    assert(r.bm > 0.0f);
    assert(fabsf(r.trim) < 0.01f);  /* no cargo = no trim */
    assert(fabsf(r.heel) < 0.01f);  /* no cargo = no heel */

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 2: Centered cargo - balanced CG */
void test_single_cargo_centered(void) {
    printf("Test 2: Single cargo centered... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "TestCargo", .weight = 500000.0f,
        .dimensions = {5.0f, 4.0f, 3.0f}, .type = "standard",
        .pos_x = 47.5f, .pos_y = 8.0f, .pos_z = 0.0f
    };

    AnalysisResult r = perform_analysis(&ship);

    assert(r.placed_item_count == 1);
    assert(r.total_cargo_weight_kg == 500000.0f);
    assert(r.cg.perc_x > 45.0f && r.cg.perc_x < 55.0f);
    assert(r.cg.perc_y > 45.0f && r.cg.perc_y < 55.0f);
    assert(!isnan(r.gm));
    assert(r.gm > 0.0f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 3: Overweight */
void test_overweight_ship(void) {
    printf("Test 3: Overweight ship... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "HeavyCargo", .weight = 9000000.0f,
        .dimensions = {10.0f, 10.0f, 5.0f}, .type = "heavy",
        .pos_x = 0.0f, .pos_y = 0.0f, .pos_z = 0.0f
    };

    AnalysisResult r = perform_analysis(&ship);
    assert(isnan(r.gm));

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 4: Forward cargo causes trim */
void test_trim_from_forward_cargo(void) {
    printf("Test 4: Trim from forward cargo... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "ForwardCargo", .weight = 500000.0f,
        .dimensions = {5.0f, 4.0f, 3.0f}, .type = "standard",
        .pos_x = 5.0f, .pos_y = 8.0f, .pos_z = 0.0f
    };

    AnalysisResult r = perform_analysis(&ship);

    /* LCG should be forward of midship (negative = forward) */
    assert(r.lcg < 0.0f);
    /* Trim should be negative (bow down) */
    assert(r.trim < 0.0f);
    /* CG should be forward (low percentage) */
    assert(r.cg.perc_x < 40.0f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 5: Off-center cargo causes heel */
void test_heel_from_offset_cargo(void) {
    printf("Test 5: Heel from off-center cargo... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    /* Cargo placed far to starboard (y >> width/2) */
    ship.cargo[0] = (Cargo){
        .id = "StarboardCargo", .weight = 500000.0f,
        .dimensions = {5.0f, 4.0f, 3.0f}, .type = "standard",
        .pos_x = 47.5f, .pos_y = 15.0f, .pos_z = 0.0f
    };

    AnalysisResult r = perform_analysis(&ship);

    /* Should have a noticeable heel (starboard = positive) */
    assert(r.heel > 0.1f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 6: Multiple cargo - no unplaced counted */
void test_multiple_cargo(void) {
    printf("Test 6: Multiple cargo items... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 3;
    ship.cargo[0] = (Cargo){
        .id = "C1", .weight = 300000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 10.0f, .pos_y = 5.0f, .pos_z = 0.0f
    };
    ship.cargo[1] = (Cargo){
        .id = "C2", .weight = 400000.0f,
        .dimensions = {5.0f, 4.0f, 3.0f}, .type = "standard",
        .pos_x = 50.0f, .pos_y = 8.0f, .pos_z = 0.0f
    };
    ship.cargo[2] = (Cargo){
        .id = "C3", .weight = 300000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 85.0f, .pos_y = 5.0f, .pos_z = 0.0f
    };

    AnalysisResult r = perform_analysis(&ship);
    assert(r.placed_item_count == 3);
    assert(r.total_cargo_weight_kg == 1000000.0f);
    assert(!isnan(r.gm));

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 7: Unplaced cargo ignored */
void test_unplaced_cargo(void) {
    printf("Test 7: Unplaced cargo handling... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 2;
    ship.cargo[0] = (Cargo){
        .id = "Placed", .weight = 300000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 50.0f, .pos_y = 10.0f, .pos_z = 0.0f
    };
    ship.cargo[1] = (Cargo){
        .id = "Unplaced", .weight = 200000.0f,
        .dimensions = {3.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = -1.0f, .pos_y = -1.0f, .pos_z = -1.0f
    };

    AnalysisResult r = perform_analysis(&ship);
    assert(r.placed_item_count == 1);
    assert(r.total_cargo_weight_kg == 300000.0f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 8: GM in reasonable range */
void test_gm_reasonable_range(void) {
    printf("Test 8: GM in reasonable range... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "MedLoad", .weight = 3000000.0f,
        .dimensions = {10.0f, 10.0f, 4.0f}, .type = "standard",
        .pos_x = 45.0f, .pos_y = 5.0f, .pos_z = -2.0f
    };

    AnalysisResult r = perform_analysis(&ship);
    assert(r.gm > 0.3f);
    assert(r.gm < 50.0f);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 9: IMO compliance for well-loaded ship */
void test_imo_compliance(void) {
    printf("Test 9: IMO compliance check... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 2;
    /* Balanced, moderate loading - should be IMO compliant */
    ship.cargo[0] = (Cargo){
        .id = "FwdLoad", .weight = 1500000.0f,
        .dimensions = {8.0f, 8.0f, 4.0f}, .type = "standard",
        .pos_x = 20.0f, .pos_y = 6.0f, .pos_z = -4.0f
    };
    ship.cargo[1] = (Cargo){
        .id = "AftLoad", .weight = 1500000.0f,
        .dimensions = {8.0f, 8.0f, 4.0f}, .type = "standard",
        .pos_x = 70.0f, .pos_y = 6.0f, .pos_z = -4.0f
    };

    AnalysisResult r = perform_analysis(&ship);

    assert(!isnan(r.gm));
    assert(r.gm >= 0.15f);  /* IMO minimum */
    assert(r.gz_at_30 >= 0.20f);
    assert(r.gz_max > 0.0f);
    assert(r.gz_max_angle >= 25.0f);
    assert(r.area_0_30 >= 0.055f);
    assert(r.area_0_40 >= 0.090f);
    assert(r.area_30_40 >= 0.030f);
    assert(r.imo_compliant == 1);

    free(ship.cargo);
    printf("PASS\n");
}

/* Test 10: Hydrostatic fields populated */
void test_hydrostatic_fields(void) {
    printf("Test 10: Hydrostatic field values... ");
    Ship ship = create_test_ship();
    ship.cargo_count = 1;
    ship.cargo[0] = (Cargo){
        .id = "Load", .weight = 2000000.0f,
        .dimensions = {10.0f, 10.0f, 4.0f}, .type = "standard",
        .pos_x = 45.0f, .pos_y = 5.0f, .pos_z = -2.0f
    };

    AnalysisResult r = perform_analysis(&ship);

    /* All hydrostatic values should be positive */
    assert(r.draft > 0.0f);
    assert(r.kb > 0.0f);
    assert(r.bm > 0.0f);
    assert(r.kg > 0.0f);
    /* KB should be less than draft */
    assert(r.kb < r.draft);
    /* GM = KB + BM - KG */
    float expected_gm = r.kb + r.bm - r.kg;
    assert(fabsf(r.gm - expected_gm) < 0.01f);

    free(ship.cargo);
    printf("PASS\n");
}

int main(void) {
    printf("\n=== Running Analysis Module Tests ===\n\n");

    test_empty_ship();
    test_single_cargo_centered();
    test_overweight_ship();
    test_trim_from_forward_cargo();
    test_heel_from_offset_cargo();
    test_multiple_cargo();
    test_unplaced_cargo();
    test_gm_reasonable_range();
    test_imo_compliance();
    test_hydrostatic_fields();

    printf("\n=== All Analysis Tests Passed! ===\n\n");
    return 0;
}
