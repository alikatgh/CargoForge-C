#include "placement_2d.h"
#include "cargoforge.h" // <<< THE MISSING LINE
#include <stdio.h>
#include <stdlib.h>

// Static comparator function for qsort. Sorts cargo by weight descending.
static int cargo_cmp_by_weight_desc(const void *a, const void *b) {
    const Cargo *ca = (const Cargo *)a;
    const Cargo *cb = (const Cargo *)b;
    if (ca->weight < cb->weight) return 1;
    if (ca->weight > cb->weight) return -1;
    return 0;
}

// In file: placement_2d.c

// Attempts to place a cargo item in a specific bin. Returns 1 on success, 0 on failure.
static int place_item_in_bin(Bin *bin, Cargo *c) {
    // Try placing in two orientations: (L, W) and (W, L)
    float orientations[2][2] = {
        {c->dimensions[0], c->dimensions[1]}, // Length x Width
        {c->dimensions[1], c->dimensions[0]}  // Width x Length
    };
    int num_orientations = (orientations[0][0] == orientations[0][1]) ? 1 : 2;

    for (int i = 0; i < num_orientations; ++i) {
        float cw = orientations[i][0]; // current width
        float ch = orientations[i][1]; // current height (in bin's 2D context)
        // 1. First, check ALL existing shelves for a fit
        for (int k = 0; k < bin->shelf_count; k++) {
            Shelf *shelf = &bin->shelves[k];
            if (ch <= shelf->height && cw <= (bin->w - shelf->used_width)) {
                c->pos_x = bin->x_offset + shelf->used_width;
                c->pos_y = bin->y_offset + shelf->y;
                c->pos_z = bin->z_offset;
                shelf->used_width += cw;
                return 1; // Success!
            }
        }
        
        // 2. ONLY if no existing shelf worked, try to create a new one
        if ((bin->used_height + ch) <= bin->h && bin->shelf_count < MAX_SHELVES) {
            Shelf new_shelf;
            new_shelf.y = bin->used_height;
            new_shelf.height = ch;
            new_shelf.used_width = cw;

            bin->shelves[bin->shelf_count++] = new_shelf;
            bin->used_height += ch;

            c->pos_x = bin->x_offset;
            c->pos_y = bin->y_offset + new_shelf.y;
            c->pos_z = bin->z_offset;

            return 1; 
        }
    }
    
    return 0; // Could not place in either orientation in this bin
}

void place_cargo_2d(Ship *ship) {
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_weight_desc);

    Bin bins[3] = {
        {"Hold1", 0.0f, 0.0f, -5.0f, ship->length / 2, ship->width, 0.0f, {{0}}, 0},
        {"Hold2", ship->length / 2, 0.0f, -5.0f, ship->length / 2, ship->width, 0.0f, {{0}}, 0},
        {"Deck", 0.0f, 0.0f, 0.0f, ship->length, ship->width, 0.0f, {{0}}, 0}
    };
    int bin_count = 3;

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        int placed = 0;
        for (int j = 0; j < bin_count; j++) {
            if (place_item_in_bin(&bins[j], c)) {
                placed = 1;
                break;
            }
        }
        if (!placed) {
            fprintf(stderr, "Warning: Could not place cargo %s\n", c->id);
        }
    }
}