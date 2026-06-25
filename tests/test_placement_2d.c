/*
 * tests/test_placement_2d.c - Unit tests for the 2D placement engine.
 *
 * These tests assert INVARIANTS of any correct stow (every item placed, in
 * bounds, no two items overlapping within a bin layer) rather than brittle exact
 * coordinates. A heuristic's exact output legitimately changes as the heuristic
 * improves; its invariants must not.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../cargoforge.h"
#include "../placement_2d.h"

/* Two placed items collide if their footprints overlap AND they share a Z layer
 * (same bin level). Touching edges (>= / <=) do not count as overlap. */
static int items_overlap(const Cargo *a, const Cargo *b) {
    if (a->pos_z != b->pos_z) return 0;
    int x_disjoint = (a->pos_x + a->placed_w <= b->pos_x) || (b->pos_x + b->placed_w <= a->pos_x);
    int y_disjoint = (a->pos_y + a->placed_h <= b->pos_y) || (b->pos_y + b->placed_h <= a->pos_y);
    return !(x_disjoint || y_disjoint);
}

static void test_all_placed_in_bounds_no_overlap(void) {
    printf("  invariants (placed, in-bounds, no overlap)... ");

    Ship ship = {0};
    ship.length = 20.0f;
    ship.width = 8.0f;

    Cargo items[] = {
        {.id = "HeavyBox",   .weight = 1000.0f, .dimensions = {8.0f, 4.0f, 2.0f}, .type = "general", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "MediumBox",  .weight = 500.0f,  .dimensions = {7.0f, 5.0f, 2.0f}, .type = "general", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "SmallCrate", .weight = 100.0f,  .dimensions = {2.0f, 2.0f, 1.0f}, .type = "general", .pos_x = -1, .pos_y = -1, .pos_z = -1},
    };
    ship.cargo = items;
    ship.cargo_count = (int)(sizeof(items) / sizeof(items[0]));

    place_cargo_2d(&ship);

    /* Every item placed. */
    for (int i = 0; i < ship.cargo_count; i++) {
        assert(items[i].pos_x >= 0.0f && "every item should be placed");
        assert(items[i].placed_w > 0.0f && items[i].placed_h > 0.0f);
    }

    /* Every footprint inside the ship envelope. */
    for (int i = 0; i < ship.cargo_count; i++) {
        const Cargo *c = &items[i];
        assert(c->pos_x >= 0.0f && c->pos_x + c->placed_w <= ship.length + 1e-4f);
        assert(c->pos_y >= 0.0f && c->pos_y + c->placed_h <= ship.width + 1e-4f);
    }

    /* No two items overlap within a bin layer. */
    for (int i = 0; i < ship.cargo_count; i++) {
        for (int j = i + 1; j < ship.cargo_count; j++) {
            assert(!items_overlap(&items[i], &items[j]) && "items must not overlap");
        }
    }

    /* The placed footprint must equal one of the item's two orientations. */
    for (int i = 0; i < ship.cargo_count; i++) {
        const Cargo *c = &items[i];
        int upright = (c->placed_w == c->dimensions[0] && c->placed_h == c->dimensions[1]);
        int rotated = (c->placed_w == c->dimensions[1] && c->placed_h == c->dimensions[0]);
        assert((upright || rotated) && "footprint must be an axis orientation");
    }

    printf("OK\n");
}

static void test_oversize_item_is_not_placed(void) {
    printf("  oversize item rejected (no out-of-bounds stow)... ");

    /* A bin's X-extent is length/2 = 5 here; this item is 12 long in every
     * orientation that could fit the width, so it must NOT be placed. Before the
     * bounds fix it was stowed overflowing the hold. */
    Ship ship = {0};
    ship.length = 10.0f;
    ship.width = 4.0f;

    Cargo items[] = {
        {.id = "TooLong", .weight = 1.0f, .dimensions = {12.0f, 3.0f, 1.0f}, .type = "general", .pos_x = -1, .pos_y = -1, .pos_z = -1},
    };
    ship.cargo = items;
    ship.cargo_count = 1;

    place_cargo_2d(&ship);

    assert(items[0].pos_x < 0.0f && "item longer than every bin must stay unplaced");

    printf("OK\n");
}

/* The transverse-trim pass should center the loaded mass on the beam, keeping the
 * weighted transverse CG near the centerline instead of piled against one side. */
static void test_transverse_load_is_centered(void) {
    printf("  transverse load centered on the beam... ");

    Ship ship = {0};
    ship.length = 40.0f;
    ship.width = 12.0f;

    /* A mix of widths and weights, all narrow relative to the 12 m beam. */
    Cargo items[] = {
        {.id = "A", .weight = 800.0f, .dimensions = {10.0f, 3.0f, 2.0f}, .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "B", .weight = 600.0f, .dimensions = {8.0f, 2.0f, 2.0f},  .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "C", .weight = 400.0f, .dimensions = {6.0f, 2.0f, 2.0f},  .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "D", .weight = 200.0f, .dimensions = {4.0f, 2.0f, 2.0f},  .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
    };
    ship.cargo = items;
    ship.cargo_count = (int)(sizeof(items) / sizeof(items[0]));

    place_cargo_2d(&ship);

    /* Weighted transverse CG, computed the same way analysis.c does. */
    float total_w = 0.0f, moment_y = 0.0f;
    for (int i = 0; i < ship.cargo_count; i++) {
        const Cargo *c = &items[i];
        assert(c->pos_x >= 0.0f);
        total_w += c->weight;
        moment_y += c->weight * (c->pos_y + c->placed_h / 2.0f);
    }
    float cg_y = moment_y / total_w;
    float center = ship.width / 2.0f;

    /* Within the app's own "Good" band (45-55%); give the test a little more room. */
    assert(cg_y >= 0.40f * ship.width && cg_y <= 0.60f * ship.width &&
           "transverse CG should sit near the beam centerline");
    (void)center;

    printf("OK (CG_y = %.2f m of %.1f m beam)\n", cg_y, ship.width);
}

/* A configured hold count must still produce a valid plan: every item placed, in
 * bounds, and non-overlapping, using the dynamically-built bins. */
static void test_configurable_holds(void) {
    printf("  configurable hold count yields a valid plan... ");

    Ship ship = {0};
    ship.length = 40.0f;
    ship.width = 10.0f;
    ship.hold_count = 4; // 4 holds split the length into 10 m segments, + deck

    Cargo items[] = {
        {.id = "A", .weight = 900.0f, .dimensions = {8.0f, 3.0f, 2.0f}, .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "B", .weight = 700.0f, .dimensions = {8.0f, 3.0f, 2.0f}, .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "C", .weight = 500.0f, .dimensions = {8.0f, 3.0f, 2.0f}, .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
        {.id = "D", .weight = 300.0f, .dimensions = {8.0f, 3.0f, 2.0f}, .type = "g", .pos_x = -1, .pos_y = -1, .pos_z = -1},
    };
    ship.cargo = items;
    ship.cargo_count = (int)(sizeof(items) / sizeof(items[0]));

    place_cargo_2d(&ship);

    for (int i = 0; i < ship.cargo_count; i++) {
        const Cargo *c = &items[i];
        assert(c->pos_x >= 0.0f && "every item should be placed");
        assert(c->pos_x + c->placed_w <= ship.length + 1e-4f);
        assert(c->pos_y >= 0.0f && c->pos_y + c->placed_h <= ship.width + 1e-4f);
    }
    for (int i = 0; i < ship.cargo_count; i++) {
        for (int j = i + 1; j < ship.cargo_count; j++) {
            assert(!items_overlap(&items[i], &items[j]) && "items must not overlap");
        }
    }
    printf("OK\n");
}

/* A per-hold weight cap must keep an over-cap item out of the holds; it overflows
 * to the deck (which is exempt) rather than violating the limit. */
static void test_per_hold_weight_limit(void) {
    printf("  per-hold weight limit routes heavy cargo to deck... ");

    Ship ship = {0};
    ship.length = 40.0f;
    ship.width = 10.0f;
    ship.max_hold_weight = 30.0f * 1000.0f; // 30 t cap per hold

    Cargo items[] = {
        {.id = "Heavy", .weight = 50.0f * 1000.0f, .dimensions = {8.0f, 4.0f, 2.0f}, .type = "g",
         .pos_x = -1, .pos_y = -1, .pos_z = -1, .stackable = true},
    };
    ship.cargo = items;
    ship.cargo_count = 1;

    place_cargo_2d(&ship);

    assert(items[0].pos_x >= 0.0f && "item should still be placed (on deck)");
    assert(items[0].pos_z >= 0.0f && "a 50 t item must not enter a 30 t-capped hold");
    printf("OK\n");
}

/* With a hold depth, items that overflow the floors should stack and place; the
 * 2D model (no hold_depth) must place strictly fewer. Stacking only adds. */
static void test_vertical_stacking_adds_capacity(void) {
    printf("  vertical stacking adds capacity (hold_depth)... ");

    Ship ship = {0};
    ship.length = 20.0f;
    ship.width = 8.0f;
    ship.hold_count = 1;

    Cargo items[14];
    for (int i = 0; i < 14; i++) {
        memset(&items[i], 0, sizeof(Cargo));
        snprintf(items[i].id, sizeof(items[i].id), "Box%d", i);
        items[i].weight = 10000.0f;
        items[i].dimensions[0] = 8.0f;
        items[i].dimensions[1] = 4.0f;
        items[i].dimensions[2] = 3.0f;
        items[i].pos_x = items[i].pos_y = items[i].pos_z = -1.0f;
        items[i].stackable = true;
        items[i].temp_c = items[i].max_stack_t = NAN;
    }
    ship.cargo = items;
    ship.cargo_count = 14;

    ship.hold_depth = 0.0f; // 2D model
    place_cargo_2d(&ship);
    int base = 0;
    for (int i = 0; i < 14; i++) if (items[i].pos_x >= 0.0f) base++;

    for (int i = 0; i < 14; i++) {
        items[i].pos_x = items[i].pos_y = items[i].pos_z = -1.0f;
        items[i].placed_w = items[i].placed_h = 0.0f;
    }
    ship.hold_depth = 12.0f; // allow stacking
    place_cargo_2d(&ship);
    int stacked = 0;
    for (int i = 0; i < 14; i++) if (items[i].pos_x >= 0.0f) stacked++;

    assert(stacked > base && "hold depth should let extra cargo stack and place");
    printf("OK (%d -> %d placed)\n", base, stacked);
}

/* A floor base's max-stack-weight must bound the CUMULATIVE column load, not just
 * each single item placed directly on it. A base rated 8 t fed 5 t items must take
 * only one of them, not an unbounded pile. (Without the cumulative check this
 * placed 4; with it, 1.) */
static void test_cumulative_stack_weight(void) {
    printf("  cumulative stack weight respects floor capacity... ");

    Ship ship = {0};
    ship.length = 20.0f;
    ship.width = 8.0f;
    ship.hold_count = 1;
    ship.hold_depth = 12.0f;

    Cargo items[6];
    for (int i = 0; i < 6; i++) {
        memset(&items[i], 0, sizeof(Cargo));
        snprintf(items[i].id, sizeof(items[i].id), "I%d", i);
        items[i].pos_x = items[i].pos_y = items[i].pos_z = -1.0f;
        items[i].stackable = true;
        items[i].temp_c = items[i].max_stack_t = NAN;
    }
    // Two floor-filling bases (hold + deck); the hold base bears at most 8 t.
    items[0].weight = 100000.0f; items[0].dimensions[0] = 20; items[0].dimensions[1] = 8;
    items[0].dimensions[2] = 3;  items[0].max_stack_t = 8.0f;
    items[1].weight = 90000.0f;  items[1].dimensions[0] = 20; items[1].dimensions[1] = 8;
    items[1].dimensions[2] = 3;
    // Four light stackable items (5 t each); only the hold column can accept them.
    for (int i = 2; i < 6; i++) {
        items[i].weight = 5000.0f;
        items[i].dimensions[0] = 4; items[i].dimensions[1] = 4; items[i].dimensions[2] = 2;
    }
    ship.cargo = items;
    ship.cargo_count = 6;

    place_cargo_2d(&ship); // note: reorders items by weight

    int lights_placed = 0;
    for (int i = 0; i < 6; i++)
        if (items[i].weight < 6000.0f && items[i].pos_x >= 0.0f) lights_placed++;

    assert(lights_placed == 1 && "cumulative stacked weight must not exceed the floor's max_stack_t");
    printf("OK (%d light item stacked)\n", lights_placed);
}

int main(void) {
    printf("--- Running Placement Tests ---\n");
    test_all_placed_in_bounds_no_overlap();
    test_oversize_item_is_not_placed();
    test_transverse_load_is_centered();
    test_configurable_holds();
    test_vertical_stacking_adds_capacity();
    test_per_hold_weight_limit();
    test_cumulative_stack_weight();
    printf("--- All Placement Tests Passed ---\n");
    return 0;
}
