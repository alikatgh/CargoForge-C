#ifndef PLACEMENT_2D_H
#define PLACEMENT_2D_H

// Represents a storage area (a hold or the deck)
typedef struct {
    char name[32];
    int width;
    int height;
    // Internal state, managed by the placement function
    int used_height;
    void* shelves; // Opaque pointer to internal shelf data
    int shelf_count;
} Bin;

// Represents a single cargo item
typedef struct {
    char name[32];
    int width;
    int height;
    int weight;
    char placed_in[32]; // Output: Populated by the placement function
} CargoItem;

/**
 * @brief Places cargo items into bins using a 2D first-fit decreasing heuristic.
 *
 * This function sorts items by weight (descending) and attempts to place them
 * into the first available bin that can accommodate them, trying both original
 * and rotated orientations. It modifies the 'items' array in-place, updating
 * the 'placed_in' field for each item.
 *
 * @param bins An array of Bin structures representing ship storage areas.
 * @param bin_count The number of bins.
 * @param items An array of CargoItem structures to be placed.
 * @param item_count The number of items.
 */
void place_cargo_2d(Bin bins[], int bin_count, CargoItem items[], int item_count);

#endif // PLACEMENT_2D_H