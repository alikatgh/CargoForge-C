// In file: tests/test_placement_2d.c

#include <stdio.h>
#include <string.h>
#include <assert.h>

// Include the headers for the modules we are testing
#include "../placement_2d.h" // Note the path to the header

// A helper function to find a cargo item by its name
static CargoItem* find_item(const char* name, CargoItem items[], int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].name, name) == 0) {
            return &items[i];
        }
    }
    return NULL;
}

int main() {
    printf("Running test: test_placement_2d...\n");

    // --- 1. Setup Test Data ---
    Bin bins[2] = {
        {"SmallHold", 10, 5, 0, NULL, 0}, // A 10x5 hold
        {"Deck", 20, 3, 0, NULL, 0}       // A 20x3 deck area
    };
    int bin_count = 2;

    CargoItem items[] = {
        // This item is heavy and large, should go to SmallHold first
        {"HeavyBox", 8, 4, 1000, ""},
        // This item is too tall for SmallHold (5), but will fit on Deck if rotated
        {"TallBox", 2, 6, 500, ""},
        // This small item should fit on a new shelf in SmallHold
        {"SmallCrate", 3, 1, 100, ""}
    };
    int item_count = sizeof(items) / sizeof(CargoItem);

    // --- 2. Run the Function to Test ---
    place_cargo_2d(bins, bin_count, items, item_count);

    // --- 3. Assert the Results ---
    // Find each item to check its placement
    CargoItem* heavy_box = find_item("HeavyBox", items, item_count);
    CargoItem* tall_box = find_item("TallBox", items, item_count);
    CargoItem* small_crate = find_item("SmallCrate", items, item_count);

    // Check that none are NULL (i.e., they were found)
    assert(heavy_box != NULL);
    assert(tall_box != NULL);
    assert(small_crate != NULL);

    // Assert that each item was placed in the expected bin
    // HeavyBox is heaviest, so it gets placed first in the first available bin.
    assert(strcmp(heavy_box->placed_in, "SmallHold") == 0);

    // TallBox is next heaviest. It's too tall (6) for SmallHold (height 5),
    // but it can be rotated to (6x2) to fit on the Deck (width 20).
    // Our logic tries bins in order, so it fails SmallHold then succeeds on Deck.
    assert(strcmp(tall_box->placed_in, "Deck") == 0);

    // SmallCrate is placed last. It fits easily in SmallHold on a new shelf.
    assert(strcmp(small_crate->placed_in, "SmallHold") == 0);

    printf("Test passed!\n");

    return 0;
}