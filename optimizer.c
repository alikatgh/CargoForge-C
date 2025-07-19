/*
 * optimizer.c - Cargo placement optimization logic.
 */
#include "cargoforge.h"

// Static comparator function for qsort. Sorts cargo by weight descending.
static int cargo_cmp_by_weight_desc(const void *a, const void *b) {
    const Cargo *ca = (const Cargo *)a;
    const Cargo *cb = (const Cargo *)b;
    if (ca->weight < cb->weight) return 1;
    if (ca->weight > cb->weight) return -1;
    return 0;
}

// Places cargo onto the ship deck using a 2D guillotine bin packing heuristic.
void optimize_cargo_placement(Ship *ship) {
    // Sort cargo by weight, heaviest first, for a better packing heuristic.
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_weight_desc);

    Rect free_rects[MAX_FREE_RECTS] = {{0.0f, 0.0f, ship->length, ship->width}};
    int free_count = 1;

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        float cw = c->dimensions[0]; // Cargo length (corresponds to ship's X-axis)
        float ch = c->dimensions[1]; // Cargo width (corresponds to ship's Y-axis)
        int best_fit_idx = -1;

        // Find the first free rectangle that can fit the cargo
        for (int j = 0; j < free_count; j++) {
            if (cw <= free_rects[j].w && ch <= free_rects[j].h) {
                best_fit_idx = j;
                break; // Use first-fit heuristic
            }
        }

        if (best_fit_idx != -1) {
            Rect fr = free_rects[best_fit_idx];
            c->pos_x = fr.x;
            c->pos_y = fr.y;

            // Overwrite the used rectangle with the last one to avoid shifting memory
            free_rects[best_fit_idx] = free_rects[--free_count];

            // Split the remaining space into two new free rectangles
            // Split along the right edge
            if (fr.w - cw > 0.01 && free_count < MAX_FREE_RECTS) {
                free_rects[free_count++] = (Rect){fr.x + cw, fr.y, fr.w - cw, fr.h};
            }
            // Split along the top edge
            if (fr.h - ch > 0.01 && free_count < MAX_FREE_RECTS) {
                free_rects[free_count++] = (Rect){fr.x, fr.y + ch, cw, fr.h - ch};
            }
        } else {
            fprintf(stderr, "Warning: Could not place cargo %s (dims %.2fm x %.2fm).\n", c->id, cw, ch);
            c->pos_x = -1.0f; // Mark as unplaced
            c->pos_y = -1.0f;
        }
    }
}