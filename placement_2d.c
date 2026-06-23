#include "placement_2d.h"
#include "cargoforge.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/* A fully-resolved placement decision. This is the single source of truth shared
 * between find_placement() (which decides) and commit_placement() (which applies):
 * the two no longer re-derive the choice independently, so they cannot drift. */
typedef struct {
    int   valid;        /* 0 = no spot found in this bin */
    float x, y;         /* absolute lower-left corner of the item */
    float w, h;         /* item extents after the chosen rotation */
    int   shelf_index;  /* >= 0: reuse this shelf;  -1: open a new shelf */
} Placement;

// --- Static Helper Prototypes ---
static int cargo_cmp_by_weight_desc(const void *a, const void *b);
static Placement find_placement(const Bin *bin, const Cargo *c);
static void commit_placement(Bin *bin, const Placement *p);
static void stack_cargo(Ship *ship);
Point calculate_cg_for_placement(const Ship *ship, const Cargo *new_item, Point new_pos, float item_w, float item_h);


void place_cargo_2d(Ship *ship) {
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_weight_desc);

    /* Build the stowage regions: `holds` below-deck holds that split the length
     * evenly fore-to-aft, plus one full-length deck on top. Configurable via the
     * `holds=` config key; 0 falls back to DEFAULT_HOLDS so existing configs are
     * unchanged. If allocation fails, fall back to a single full-length hold on
     * the stack so we still produce a plan. */
    int holds = (ship->hold_count > 0) ? ship->hold_count : DEFAULT_HOLDS;
    int bin_count = holds + 1; // + deck
    Bin fallback[1];
    Bin *bins = malloc((size_t)bin_count * sizeof(Bin));
    if (!bins) {
        bins = fallback;
        bin_count = 1;
        bins[0] = (Bin){"Hold1", 0.0f, 0.0f, -5.0f, ship->length, ship->width, 0.0f, {{0}}, 0};
        holds = 1;
    } else {
        float hold_w = ship->length / (float)holds;
        for (int i = 0; i < holds; i++) {
            bins[i] = (Bin){"", i * hold_w, 0.0f, -5.0f, hold_w, ship->width, 0.0f, {{0}}, 0};
            snprintf(bins[i].name, sizeof(bins[i].name), "Hold%d", i + 1);
        }
        bins[holds] = (Bin){"Deck", 0.0f, 0.0f, 0.0f, ship->length, ship->width, 0.0f, {{0}}, 0};
    }

    /* Remember which bin each item landed in, so the transverse-trim pass below
     * can shift a whole bin's load as a unit. Index aligns with the (now sorted)
     * ship->cargo array. If the allocation fails we simply skip trimming. */
    int *bin_of = malloc((size_t)ship->cargo_count * sizeof(int));
    // Running weight per bin, to enforce an optional per-hold weight cap.
    float *bin_weight = calloc((size_t)bin_count, sizeof(float));

    Point ideal_cg = {ship->length / 2.0f, ship->width / 2.0f};

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];

        Placement best = {0};
        int best_bin_idx = -1;
        double min_dist = DBL_MAX;

        for (int j = 0; j < bin_count; j++) {
            // Respect a per-hold weight cap (deck, z=0, is exempt as open overflow).
            if (ship->max_hold_weight > 0.0f && bins[j].z_offset < 0.0f && bin_weight &&
                bin_weight[j] + c->weight > ship->max_hold_weight)
                continue;

            Placement p = find_placement(&bins[j], c);
            if (!p.valid) continue;

            Point pos = {p.x, p.y};
            Point prospective_cg = calculate_cg_for_placement(ship, c, pos, p.w, p.h);
            double dist = pow(prospective_cg.x - ideal_cg.x, 2) + pow(prospective_cg.y - ideal_cg.y, 2);

            if (dist < min_dist) {
                min_dist = dist;
                best = p;
                best_bin_idx = j;
            }
        }

        if (best_bin_idx != -1) {
            c->pos_x = best.x;
            c->pos_y = best.y;
            c->pos_z = bins[best_bin_idx].z_offset;
            c->placed_w = best.w; // Store final dimensions (post-rotation)
            c->placed_h = best.h;
            commit_placement(&bins[best_bin_idx], &best);
            if (bin_weight) bin_weight[best_bin_idx] += c->weight;
        } else {
            fprintf(stderr, "Warning: Could not place cargo %s\n", c->id);
        }
        if (bin_of) bin_of[i] = best_bin_idx;
    }
    free(bin_weight);

    /* Transverse trim: the shelf packer stacks rows from y=0 upward, which piles
     * the load against one side of the beam and skews the transverse CG. Center
     * each bin's loaded band (height `used_height`) on the beam centerline by
     * shifting every item in that bin by the same amount. A uniform per-bin shift
     * preserves all relative positions, so it can neither create overlaps nor move
     * anything out of bounds (0 <= shift and shift + used_height <= bin height). */
    if (bin_of) {
        for (int b = 0; b < bin_count; b++) {
            float shift = (bins[b].h - bins[b].used_height) / 2.0f;
            if (shift <= 0.0f) continue;
            for (int i = 0; i < ship->cargo_count; i++) {
                if (bin_of[i] == b) ship->cargo[i].pos_y += shift;
            }
        }
        free(bin_of);
    }

    if (bins != fallback) free(bins);

    stack_cargo(ship); // vertical stacking pass (no-op unless hold_depth is set)
}

// THIS IS THE FINAL, CORRECTED CG CALCULATION
Point calculate_cg_for_placement(const Ship *ship, const Cargo *new_item, Point new_pos, float item_w, float item_h) {
    float total_weight = new_item->weight;
    float moment_x = new_item->weight * (new_pos.x + item_w / 2.0f);
    float moment_y = new_item->weight * (new_pos.y + item_h / 2.0f);

    for (int i = 0; i < ship->cargo_count; ++i) {
        // Only include items that have ALREADY been placed in this run
        if (ship->cargo[i].pos_x >= 0) {
            total_weight += ship->cargo[i].weight;
            // Use the stored placed_w and placed_h for accurate calculations
            moment_x += ship->cargo[i].weight * (ship->cargo[i].pos_x + ship->cargo[i].placed_w / 2.0f);
            moment_y += ship->cargo[i].weight * (ship->cargo[i].pos_y + ship->cargo[i].placed_h / 2.0f);
        }
    }

    Point cg = {0.0f, 0.0f};
    if (total_weight > 0.01f) {
        cg.x = (moment_x / total_weight);
        cg.y = (moment_y / total_weight);
    }
    return cg;
}

static int cargo_cmp_by_weight_desc(const void *a, const void *b) {
    const Cargo *ca = (const Cargo *)a; const Cargo *cb = (const Cargo *)b;
    if (ca->weight < cb->weight) return 1; if (ca->weight > cb->weight) return -1; return 0;
}

/* Decide where item `c` would go in `bin`, if anywhere. Tries each rotation,
 * preferring an existing shelf (denser packing) over opening a new one. EVERY
 * candidate is checked against BOTH bin axes: the X-extent (`cw <= remaining
 * width`) and the Y-extent (`ch` fits the shelf height, or a new shelf fits in
 * the remaining bin height). The new-shelf branch's missing X-extent check was a
 * real out-of-bounds bug — an item longer than the hold was "placed" anyway. */
static Placement find_placement(const Bin *bin, const Cargo *c) {
    float orientations[2][2] = {{c->dimensions[0], c->dimensions[1]},
                                {c->dimensions[1], c->dimensions[0]}};
    int num_orientations = (orientations[0][0] == orientations[0][1]) ? 1 : 2;

    for (int i = 0; i < num_orientations; ++i) {
        float cw = orientations[i][0];
        float ch = orientations[i][1];

        /* Prefer reusing an existing shelf. */
        for (int k = 0; k < bin->shelf_count; k++) {
            const Shelf *shelf = &bin->shelves[k];
            if (ch <= shelf->height && cw <= (bin->w - shelf->used_width)) {
                return (Placement){1, bin->x_offset + shelf->used_width,
                                   bin->y_offset + shelf->y, cw, ch, k};
            }
        }

        /* Otherwise open a new shelf — only if the item fits in BOTH axes. */
        if (cw <= bin->w && (bin->used_height + ch) <= bin->h && bin->shelf_count < MAX_SHELVES) {
            return (Placement){1, bin->x_offset,
                               bin->y_offset + bin->used_height, cw, ch, -1};
        }
    }
    return (Placement){0, 0, 0, 0, 0, -1};
}

/* Apply a decision produced by find_placement(). No re-derivation: it trusts the
 * shelf_index the finder chose, so the two can never disagree. */
static void commit_placement(Bin *bin, const Placement *p) {
    if (p->shelf_index >= 0) {
        bin->shelves[p->shelf_index].used_width += p->w;
        return;
    }
    /* New shelf. find_placement already verified it fits and there is room. */
    Shelf new_shelf = {bin->used_height, p->h, p->w};
    bin->shelves[bin->shelf_count++] = new_shelf;
    bin->used_height += p->h;
}

/* Vertical stacking pass — runs only when ship->hold_depth > 0, so the 2D model is
 * untouched by default. Each still-unplaced item is stacked on top of an already-
 * placed, stackable item in a hold column whose footprint can support it, within
 * the hold's clear height and the base's max-stack-weight. It is purely additive:
 * no placed item is ever moved, so existing placements (and tests) are unchanged.
 * Stacked items get a higher pos_z, which correctly raises KG in the analysis. */
static void stack_cargo(Ship *ship) {
    if (ship->hold_depth <= 0.0f) return;

    /* Per item: the current top of its column, and that column's hold-floor z (for
     * the clear-height budget). Only items in a hold column (floor z < 0) are bases. */
    float *col_top = malloc((size_t)ship->cargo_count * sizeof(float));
    float *floor_z = malloc((size_t)ship->cargo_count * sizeof(float));
    if (!col_top || !floor_z) { free(col_top); free(floor_z); return; }
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        col_top[i] = (c->pos_x >= 0.0f) ? c->pos_z + c->dimensions[2] : 0.0f;
        floor_z[i] = c->pos_z;
    }

    for (int u = 0; u < ship->cargo_count; u++) {
        Cargo *item = &ship->cargo[u];
        if (item->pos_x >= 0.0f) continue; // already placed on a floor

        int best = -1;
        float best_fw = 0.0f, best_fh = 0.0f;
        for (int b = 0; b < ship->cargo_count; b++) {
            const Cargo *base = &ship->cargo[b];
            if (base->pos_x < 0.0f || floor_z[b] >= 0.0f) continue; // placed hold columns only
            if (!base->stackable || base->fragile) continue;

            // Footprint must fit on the base (try both orientations).
            float w0 = item->dimensions[0], h0 = item->dimensions[1], fw, fh;
            if (w0 <= base->placed_w && h0 <= base->placed_h) { fw = w0; fh = h0; }
            else if (h0 <= base->placed_w && w0 <= base->placed_h) { fw = h0; fh = w0; }
            else continue;

            // Respect the base's max-stack-weight (if specified).
            if (!isnan(base->max_stack_t) && item->weight / 1000.0f > base->max_stack_t) continue;

            // Clear-height budget: stack top above the column's hold floor.
            if ((col_top[b] + item->dimensions[2]) - floor_z[b] > ship->hold_depth) continue;

            // Prefer the lowest available column top (denser, lower KG).
            if (best < 0 || col_top[b] < col_top[best]) { best = b; best_fw = fw; best_fh = fh; }
        }

        if (best >= 0) {
            Cargo *base = &ship->cargo[best];
            item->pos_x = base->pos_x;
            item->pos_y = base->pos_y;
            item->pos_z = col_top[best];        // rest on the current column top
            item->placed_w = best_fw;
            item->placed_h = best_fh;
            float top = item->pos_z + item->dimensions[2];
            col_top[best] = top;                // column grows
            col_top[u] = top;                   // this item can now bear more
            floor_z[u] = floor_z[best];         // same column floor
        }
    }
    free(col_top);
    free(floor_z);
}
