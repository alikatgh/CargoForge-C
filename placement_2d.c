#include "placement_2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SHELVES_PER_BIN 100

// Internal structure for a shelf, not exposed in the header
typedef struct {
    int height;
    int remaining_width;
} Shelf;

// Comparison function for qsort (sort by weight descending)
static int compare_cargo(const void *a, const void *b) {
    const CargoItem *itemA = (const CargoItem *)a;
    const CargoItem *itemB = (const CargoItem *)b;
    return itemB->weight - itemA->weight; // Descending order
}

// Internal helper to attempt placing an item in a single bin, with rotation
static int place_item_in_bin(Bin *bin, const CargoItem *item) {
    // Try placing in two orientations: (width, height) and (height, width)
    int orientations[2][2] = {
        {item->width, item->height},
        {item->height, item->width}
    };
    int num_orientations = (item->width == item->height) ? 1 : 2; // No need to rotate squares

    for (int i = 0; i < num_orientations; i++) {
        int w = orientations[i][0];
        int h = orientations[i][1];

        // 1. Try to fit on an existing shelf
        for (int j = 0; j < bin->shelf_count; j++) {
            Shelf *shelf = &(((Shelf *)bin->shelves)[j]);
            if (h <= shelf->height && w <= shelf->remaining_width) {
                shelf->remaining_width -= w;
                return 1; // Placed successfully
            }
        }

        // 2. Try to create a new shelf if there's vertical space
        if (bin->used_height + h <= bin->height && bin->shelf_count < MAX_SHELVES_PER_BIN) {
            Shelf *new_shelf = &(((Shelf *)bin->shelves)[bin->shelf_count++]);
            new_shelf->height = h;
            new_shelf->remaining_width = bin->width - w;
            bin->used_height += h;
            return 1; // Placed successfully
        }
    }

    return 0; // Could not place in this bin
}

// Main placement function exposed in the header
void place_cargo_2d(Bin bins[], int bin_count, CargoItem items[], int item_count) {
    // Allocate memory for shelves within each bin
    for (int i = 0; i < bin_count; i++) {
        bins[i].shelves = calloc(MAX_SHELVES_PER_BIN, sizeof(Shelf));
        if (bins[i].shelves == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for shelves.\n");
            return; // Handle memory allocation failure
        }
        bins[i].shelf_count = 0;
        bins[i].used_height = 0;
    }

    // Sort items by weight, descending, for stability
    qsort(items, item_count, sizeof(CargoItem), compare_cargo);

    // Main placement loop
    for (int i = 0; i < item_count; i++) {
        int placed = 0;
        for (int j = 0; j < bin_count; j++) {
            if (place_item_in_bin(&bins[j], &items[i])) {
                strcpy(items[i].placed_in, bins[j].name);
                placed = 1;
                break; // Move to the next item
            }
        }
        if (!placed) {
            strcpy(items[i].placed_in, "Not Placed");
        }
    }

    // Free the allocated shelf memory for each bin
    for (int i = 0; i < bin_count; i++) {
        free(bins[i].shelves);
        bins[i].shelves = NULL; // Avoid dangling pointers
    }
}