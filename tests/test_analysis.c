/*
 * test_analysis.c - Unit tests for analysis module
 *
 * Tests stability calculations, CG calculations, and analysis reporting.
 */

#include "../cargoforge.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

// Test helper: create a minimal ship for testing
Ship create_test_ship(void) {
    Ship ship = {
        .length = 100.0f,
        .width = 20.0f,
        .max_weight = 10000000.0f,  // 10,000 tonnes
        .lightship_weight = 2000000.0f,  // 2,000 tonnes
        .lightship_kg = 5.0f,  // 5m above keel
        .cargo_count = 0,
        .cargo_capacity = 10,
        .cargo = NULL
    };
    ship.cargo = malloc(ship.cargo_capacity * sizeof(Cargo));
    return ship;
}

// Test 1: Empty ship analysis
void test_empty_ship(void) {
    printf("Test 1: Empty ship analysis... ");

    Ship ship = create_test_ship();
    AnalysisResult result = perform_analysis(&ship);

    assert(result.placed_item_count == 0);
    assert(result.total_cargo_weight_kg == 0.0f);
    assert(!isnan(result.gm));  // Should have valid GM even with no cargo
    assert(result.gm > 0.0f);   // Should be stable

    free(ship.cargo);
    printf("PASS\n");
}

// Test 2: Single cargo item - centered
void test_single_cargo_centered(void) {
    printf("Test 2: Single cargo centered... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;

    // Place cargo at ship center
    Cargo c = {
        .id = "TestCargo",
        .weight = 500000.0f,  // 500 tonnes
        .dimensions = {5.0f, 4.0f, 3.0f},
        .type = "standard",
        .pos_x = 47.5f,  // Center X (50 - 2.5)
        .pos_y = 8.0f,   // Center Y (10 - 2)
        .pos_z = 0.0f
    };
    ship.cargo[0] = c;

    AnalysisResult result = perform_analysis(&ship);

    assert(result.placed_item_count == 1);
    assert(result.total_cargo_weight_kg == 500000.0f);

    // CG should be near center (around 50% in both directions)
    assert(result.cg.perc_x > 45.0f && result.cg.perc_x < 55.0f);
    assert(result.cg.perc_y > 45.0f && result.cg.perc_y < 55.0f);

    assert(!isnan(result.gm));
    assert(result.gm > 0.0f);  // Should be stable

    free(ship.cargo);
    printf("PASS\n");
}

// Test 3: Overweight ship
void test_overweight_ship(void) {
    printf("Test 3: Overweight ship... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;

    // Cargo exceeds max weight
    Cargo c = {
        .id = "HeavyCargo",
        .weight = 9000000.0f,  // 9,000 tonnes (ship max 10,000, lightship 2,000)
        .dimensions = {10.0f, 10.0f, 5.0f},
        .type = "heavy",
        .pos_x = 0.0f,
        .pos_y = 0.0f,
        .pos_z = 0.0f
    };
    ship.cargo[0] = c;

    AnalysisResult result = perform_analysis(&ship);

    // Should detect overweight and return NaN for GM
    assert(isnan(result.gm));

    free(ship.cargo);
    printf("PASS\n");
}

// Test 4: Unbalanced cargo (off-center)
void test_unbalanced_cargo(void) {
    printf("Test 4: Unbalanced cargo placement... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;

    // Cargo at far forward position
    Cargo c = {
        .id = "ForwardCargo",
        .weight = 500000.0f,
        .dimensions = {5.0f, 4.0f, 3.0f},
        .type = "standard",
        .pos_x = 5.0f,   // Very forward
        .pos_y = 8.0f,
        .pos_z = 0.0f
    };
    ship.cargo[0] = c;

    AnalysisResult result = perform_analysis(&ship);

    // CG should be forward (low percentage)
    assert(result.cg.perc_x < 40.0f);

    free(ship.cargo);
    printf("PASS\n");
}

// Test 5: Multiple cargo items
void test_multiple_cargo(void) {
    printf("Test 5: Multiple cargo items... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 3;

    // Three cargo items at different positions
    ship.cargo[0] = (Cargo){
        .id = "Cargo1", .weight = 300000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 10.0f, .pos_y = 5.0f, .pos_z = 0.0f
    };
    ship.cargo[1] = (Cargo){
        .id = "Cargo2", .weight = 400000.0f,
        .dimensions = {5.0f, 4.0f, 3.0f}, .type = "standard",
        .pos_x = 50.0f, .pos_y = 8.0f, .pos_z = 0.0f
    };
    ship.cargo[2] = (Cargo){
        .id = "Cargo3", .weight = 300000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 85.0f, .pos_y = 5.0f, .pos_z = 0.0f
    };

    AnalysisResult result = perform_analysis(&ship);

    assert(result.placed_item_count == 3);
    assert(result.total_cargo_weight_kg == 1000000.0f);
    assert(!isnan(result.gm));

    free(ship.cargo);
    printf("PASS\n");
}

// Test 6: Unplaced cargo (negative positions)
void test_unplaced_cargo(void) {
    printf("Test 6: Unplaced cargo handling... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 2;

    // First cargo placed, second unplaced
    ship.cargo[0] = (Cargo){
        .id = "Placed", .weight = 300000.0f,
        .dimensions = {4.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = 50.0f, .pos_y = 10.0f, .pos_z = 0.0f
    };
    ship.cargo[1] = (Cargo){
        .id = "Unplaced", .weight = 200000.0f,
        .dimensions = {3.0f, 3.0f, 2.0f}, .type = "standard",
        .pos_x = -1.0f, .pos_y = -1.0f, .pos_z = -1.0f  // Unplaced marker
    };

    AnalysisResult result = perform_analysis(&ship);

    // Should only count placed cargo
    assert(result.placed_item_count == 1);
    assert(result.total_cargo_weight_kg == 300000.0f);

    free(ship.cargo);
    printf("PASS\n");
}

// Test 7: GM calculation reasonableness
void test_gm_reasonable_range(void) {
    printf("Test 7: GM calculation in reasonable range... ");

    Ship ship = create_test_ship();
    ship.cargo_count = 1;

    // Load ship to 50% capacity
    Cargo c = {
        .id = "MediumLoad",
        .weight = 3000000.0f,  // 3,000 tonnes (total 5,000t with lightship)
        .dimensions = {10.0f, 10.0f, 4.0f},
        .type = "standard",
        .pos_x = 45.0f,
        .pos_y = 5.0f,
        .pos_z = -2.0f  // Below deck
    };
    ship.cargo[0] = c;

    AnalysisResult result = perform_analysis(&ship);

    // GM should be positive
    // Note: Our simplified model can produce high GM values due to box-hull approximation
    // Real ships would need more sophisticated hull modeling for accurate GM
    assert(result.gm > 0.3f);
    assert(result.gm < 50.0f);  // Sanity check only

    free(ship.cargo);
    printf("PASS\n");
}

int main(void) {
    printf("\n=== Running Analysis Module Tests ===\n\n");

    test_empty_ship();
    test_single_cargo_centered();
    test_overweight_ship();
    test_unbalanced_cargo();
    test_multiple_cargo();
    test_unplaced_cargo();
    test_gm_reasonable_range();

    printf("\n=== All Analysis Tests Passed! ===\n\n");
    return 0;
}
