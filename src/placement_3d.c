/*
 * placement_3d.c - 3D bin-packing implementation
 *
 * Implements guillotine-based 3D bin-packing for realistic cargo placement.
 */

#include "placement_3d.h"
#include "constraints.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Comparator for sorting cargo by volume (descending)
static int cargo_cmp_by_volume_desc(const void *a, const void *b) {
    const Cargo *ca = (const Cargo *)a;
    const Cargo *cb = (const Cargo *)b;
    float vol_a = ca->dimensions[0] * ca->dimensions[1] * ca->dimensions[2];
    float vol_b = cb->dimensions[0] * cb->dimensions[1] * cb->dimensions[2];
    if (vol_a < vol_b) return 1;
    if (vol_a > vol_b) return -1;
    return 0;
}

// Get cargo dimensions for a specific orientation (0-5)
static void get_orientation_dims(const Cargo *c, int orientation,
                                 float *w, float *d, float *h) {
    float dims[3] = {c->dimensions[0], c->dimensions[1], c->dimensions[2]};

    // All 6 possible orientations (permutations of XYZ)
    switch (orientation) {
        case 0: *w = dims[0]; *d = dims[1]; *h = dims[2]; break; // XYZ
        case 1: *w = dims[0]; *d = dims[2]; *h = dims[1]; break; // XZY
        case 2: *w = dims[1]; *d = dims[0]; *h = dims[2]; break; // YXZ
        case 3: *w = dims[1]; *d = dims[2]; *h = dims[0]; break; // YZX
        case 4: *w = dims[2]; *d = dims[0]; *h = dims[1]; break; // ZXY
        case 5: *w = dims[2]; *d = dims[1]; *h = dims[0]; break; // ZYX
        default: *w = dims[0]; *d = dims[1]; *h = dims[2]; break;
    }
}

// Check if cargo fits in space with given orientation
static int fits_in_space(const Space3D *space, const Cargo *cargo, int orientation) {
    float w, d, h;
    get_orientation_dims(cargo, orientation, &w, &d, &h);
    return (w <= space->width && d <= space->depth && h <= space->height);
}

// Calculate volume of a space
static float space_volume(const Space3D *space) {
    return space->width * space->depth * space->height;
}

// Forward declaration needed for constraint checking
extern Ship *g_current_ship;

int find_best_fit_3d(Bin3D *bins, int bin_count, const Cargo *cargo,
                     int *best_bin, int *best_space, int *best_orientation) {
    *best_bin = -1;
    *best_space = -1;
    *best_orientation = -1;
    float best_fit_volume = 1e9f;  // Find tightest fit (minimize wasted space)

    for (int b = 0; b < bin_count; b++) {
        Bin3D *bin = &bins[b];

        // Check weight constraint
        if (bin->current_weight + cargo->weight > bin->max_weight) {
            continue;
        }

        for (int s = 0; s < bin->space_count; s++) {
            Space3D *space = &bin->spaces[s];
            if (!space->is_free) continue;

            // Check cargo-specific constraints
            if (g_current_ship && !check_cargo_constraints(g_current_ship, cargo, bin, space)) {
                continue;  // Constraint violation, skip this placement
            }

            // Try all 6 orientations
            for (int o = 0; o < 6; o++) {
                if (fits_in_space(space, cargo, o)) {
                    float vol = space_volume(space);
                    // Prefer smaller spaces (tighter fit = less waste)
                    if (vol < best_fit_volume) {
                        best_fit_volume = vol;
                        *best_bin = b;
                        *best_space = s;
                        *best_orientation = o;
                    }
                }
            }
        }
    }

    return (*best_bin != -1);
}

void split_space_3d(Bin3D *bin, int space_idx, const Cargo *cargo, int orientation) {
    if (bin->space_count >= MAX_FREE_RECTS - 3) {
        fprintf(stderr, "Warning: Max free rects reached in bin %s\n", bin->name);
        return;
    }

    Space3D *original = &bin->spaces[space_idx];
    float cargo_w, cargo_d, cargo_h;
    get_orientation_dims(cargo, orientation, &cargo_w, &cargo_d, &cargo_h);

    // Mark original space as occupied
    original->is_free = 0;

    // Guillotine split: create up to 3 new spaces
    // Right remainder (along width)
    if (original->width > cargo_w) {
        Space3D right = {
            .x = original->x + cargo_w,
            .y = original->y,
            .z = original->z,
            .width = original->width - cargo_w,
            .depth = original->depth,
            .height = original->height,
            .is_free = 1
        };
        bin->spaces[bin->space_count++] = right;
    }

    // Back remainder (along depth)
    if (original->depth > cargo_d) {
        Space3D back = {
            .x = original->x,
            .y = original->y + cargo_d,
            .z = original->z,
            .width = cargo_w,  // Only the width of placed cargo
            .depth = original->depth - cargo_d,
            .height = original->height,
            .is_free = 1
        };
        bin->spaces[bin->space_count++] = back;
    }

    // Top remainder (along height)
    if (original->height > cargo_h) {
        Space3D top = {
            .x = original->x,
            .y = original->y,
            .z = original->z + cargo_h,
            .width = cargo_w,  // Only the width of placed cargo
            .depth = cargo_d,  // Only the depth of placed cargo
            .height = original->height - cargo_h,
            .is_free = 1
        };
        bin->spaces[bin->space_count++] = top;
    }
}

// Global pointer for constraint checking (temporary solution)
Ship *g_current_ship = NULL;

void place_cargo_3d(Ship *ship) {
    // Set global pointer for constraint checking
    g_current_ship = ship;

    // Sort cargo by volume (largest first - FFD heuristic)
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_volume_desc);

    // Define realistic bins for a typical cargo ship
    // Using ship dimensions to create proportional holds
    Bin3D bins[3];
    int bin_count = 3;

    // Forward hold (30% of length)
    bins[0] = (Bin3D){
        .name = "ForwardHold",
        .x = 0.0f, .y = 0.0f, .z = -8.0f,  // Below waterline
        .width = ship->length * 0.3f,
        .depth = ship->width * 0.8f,  // Leave space for side tanks
        .height = 8.0f,  // Typical hold height
        .max_weight = ship->max_weight * 0.3f,
        .current_weight = 0.0f,
        .space_count = 1
    };
    bins[0].spaces[0] = (Space3D){
        .x = 0.0f, .y = 0.0f, .z = -8.0f,
        .width = bins[0].width,
        .depth = bins[0].depth,
        .height = bins[0].height,
        .is_free = 1
    };

    // Aft hold (30% of length)
    bins[1] = (Bin3D){
        .name = "AftHold",
        .x = ship->length * 0.7f, .y = 0.0f, .z = -8.0f,
        .width = ship->length * 0.3f,
        .depth = ship->width * 0.8f,
        .height = 8.0f,
        .max_weight = ship->max_weight * 0.3f,
        .current_weight = 0.0f,
        .space_count = 1
    };
    bins[1].spaces[0] = (Space3D){
        .x = ship->length * 0.7f, .y = 0.0f, .z = -8.0f,
        .width = bins[1].width,
        .depth = bins[1].depth,
        .height = bins[1].height,
        .is_free = 1
    };

    // Deck (full length, lower weight capacity)
    bins[2] = (Bin3D){
        .name = "Deck",
        .x = 0.0f, .y = 0.0f, .z = 0.0f,  // At waterline
        .width = ship->length,
        .depth = ship->width,
        .height = 4.0f,  // Lower stacking on deck
        .max_weight = ship->max_weight * 0.4f,
        .current_weight = 0.0f,
        .space_count = 1
    };
    bins[2].spaces[0] = (Space3D){
        .x = 0.0f, .y = 0.0f, .z = 0.0f,
        .width = bins[2].width,
        .depth = bins[2].depth,
        .height = bins[2].height,
        .is_free = 1
    };

    // Place each cargo item
    int placed_count = 0;
    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        int best_bin, best_space, best_orientation;

        if (find_best_fit_3d(bins, bin_count, c, &best_bin, &best_space, &best_orientation)) {
            Bin3D *bin = &bins[best_bin];
            Space3D *space = &bin->spaces[best_space];

            // Set cargo position
            c->pos_x = space->x;
            c->pos_y = space->y;
            c->pos_z = space->z;

            // Update bin weight
            bin->current_weight += c->weight;

            // Split the space
            split_space_3d(bin, best_space, c, best_orientation);

            placed_count++;
        } else {
            fprintf(stderr, "Warning: Could not place cargo %s (%.1f x %.1f x %.1f m, %.1f kg)\n",
                    c->id, c->dimensions[0], c->dimensions[1], c->dimensions[2], c->weight);
            // Mark as unplaced
            c->pos_x = c->pos_y = c->pos_z = -1.0f;
        }
    }

    // Print placement summary
    fprintf(stderr, "3D Placement complete: %d/%d items placed\n", placed_count, ship->cargo_count);
    for (int b = 0; b < bin_count; b++) {
        fprintf(stderr, "  %s: %.1f / %.1f kg (%.1f%% capacity)\n",
                bins[b].name, bins[b].current_weight, bins[b].max_weight,
                (bins[b].current_weight / bins[b].max_weight) * 100.0f);
    }
}
