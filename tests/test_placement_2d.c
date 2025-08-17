// In file: tests/test_placement_2d.c

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../cargoforge.h"
#include "../placement_2d.h"

static Cargo* find_item(const char* name, Cargo items[], int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].id, name) == 0) {
            return &items[i];
        }
    }
    return NULL;
}

int main() {
    printf("Running test: test_placement_2d...\n");

    // --- 1. Setup Test Data ---
    Ship test_ship = {0};
    test_ship.length = 20.0f; // Hold1: x=[0,10), Hold2: x=[10,20)
    test_ship.width = 8.0f;   // This is the "height" of a bin's 2D space

    Cargo items[] = {
        {"HeavyBox", 1000.0f, {8.0f, 4.0f, 2.0f}, "general", -1, -1, -1},
        {"MediumBox", 500.0f, {7.0f, 5.0f, 2.0f}, "general", -1, -1, -1},
        {"SmallCrate", 100.0f, {2.0f, 2.0f, 1.0f}, "general", -1, -1, -1}
    };
    
    test_ship.cargo = items;
    test_ship.cargo_count = sizeof(items) / sizeof(Cargo);

    // --- 2. Run the Function to Test ---
    place_cargo_2d(&test_ship);

    // --- 3. Assert the Results ---
    Cargo* heavy_box = find_item("HeavyBox", test_ship.cargo, test_ship.cargo_count);
    Cargo* medium_box = find_item("MediumBox", test_ship.cargo, test_ship.cargo_count);
    Cargo* small_crate = find_item("SmallCrate", test_ship.cargo, test_ship.cargo_count);

    assert(heavy_box != NULL && heavy_box->pos_x >= 0);
    assert(medium_box != NULL && medium_box->pos_x >= 0);
    assert(small_crate != NULL && small_crate->pos_x >= 0);

    // HeavyBox is heaviest, goes to Hold1. pos_x should be 0.
    assert(heavy_box->pos_x < 10.0f && heavy_box->pos_z < 0);
    
    // MediumBox is next. It's too tall to fit on a new shelf in Hold1 (4+5 > 8).
    // So it MUST be placed in Hold2. Its pos_x should be 10.
    assert(medium_box->pos_x >= 10.0f && medium_box->pos_z < 0);

    // SmallCrate is last. It should fit on the same shelf as HeavyBox in Hold1.
    assert(small_crate->pos_x < 10.0f && small_crate->pos_z < 0 && small_crate->pos_x > 0);

    printf("✅ Test passed!\n");

    return 0;
}