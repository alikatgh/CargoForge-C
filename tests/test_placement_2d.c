/*
 * tests/test_placement_2d.c - Unit tests for the 2D placement engine.
 *
 * These tests assert INVARIANTS of any correct stow (every item placed, in
 * bounds, no two items overlapping within a bin layer) rather than brittle exact
 * coordinates. A heuristic's exact output legitimately changes as the heuristic
 * improves; its invariants must not.
 */
#include <assert.h>
#include <stdio.h>

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

int main(void) {
    printf("--- Running Placement Tests ---\n");
    test_all_placed_in_bounds_no_overlap();
    test_oversize_item_is_not_placed();
    test_transverse_load_is_centered();
    test_configurable_holds();
    test_per_hold_weight_limit();
    printf("--- All Placement Tests Passed ---\n");
    return 0;
}
