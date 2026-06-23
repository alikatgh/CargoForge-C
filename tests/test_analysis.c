/*
 * tests/test_analysis.c - Unit tests for the stability analysis (CG + GM).
 *
 * The analysis is the safety-critical core: a wrong center-of-gravity or
 * metacentric height is the difference between a stable ship and a capsized one.
 * These tests pin the math against hand-computed values.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../cargoforge.h"

static int approx(float a, float b, float tol) { return fabsf(a - b) <= tol; }

/* A single centered item on a large, stable hull. Hand-computed expectations:
 *   CG long/trans = 50% / 50%,  weight = 100 t,  GM > 0 (stable). */
static void test_centered_load_is_balanced_and_stable(void) {
    printf("  centered load -> 50/50 CG, positive GM... ");

    Ship ship = {0};
    ship.length = 100.0f;
    ship.width = 20.0f;
    ship.max_weight = 10000.0f * 1000.0f;       /* 10,000 t */
    ship.lightship_weight = 1000.0f * 1000.0f;  /* 1,000 t  */
    ship.lightship_kg = 5.0f;

    Cargo item = {
        .id = "Centered", .weight = 100.0f * 1000.0f, .dimensions = {10.0f, 4.0f, 4.0f},
        .type = "general", .pos_x = 45.0f, .pos_y = 8.0f, .pos_z = -2.0f,
        .placed_w = 10.0f, .placed_h = 4.0f,
    };
    ship.cargo = &item;
    ship.cargo_count = 1;

    AnalysisResult r = perform_analysis(&ship);

    assert(r.placed_item_count == 1);
    assert(approx(r.total_cargo_weight_kg, 100000.0f, 1.0f));
    assert(approx(r.cg.perc_x, 50.0f, 0.1f) && "longitudinal CG should be amidships");
    assert(approx(r.cg.perc_y, 50.0f, 0.1f) && "transverse CG should be centerline");
    assert(!isnan(r.gm) && r.gm > 0.0f && "a centered load should be stable");

    printf("OK\n");
}

/* Cargo heavier than the ship's max weight must be rejected (GM = NaN). */
static void test_overweight_plan_is_rejected(void) {
    printf("  overweight plan -> GM = NaN... ");

    Ship ship = {0};
    ship.length = 100.0f;
    ship.width = 20.0f;
    ship.max_weight = 100.0f * 1000.0f;         /* only 100 t allowed */
    ship.lightship_weight = 50.0f * 1000.0f;

    Cargo item = {
        .id = "TooHeavy", .weight = 200.0f * 1000.0f, .dimensions = {10.0f, 4.0f, 4.0f},
        .type = "general", .pos_x = 45.0f, .pos_y = 8.0f, .pos_z = -2.0f,
        .placed_w = 10.0f, .placed_h = 4.0f,
    };
    ship.cargo = &item;
    ship.cargo_count = 1;

    AnalysisResult r = perform_analysis(&ship);
    assert(isnan(r.gm) && "overweight plan must be flagged invalid");

    printf("OK\n");
}

/* Unplaced cargo (pos_x < 0) must be excluded from the analysis entirely. */
static void test_unplaced_cargo_is_excluded(void) {
    printf("  unplaced cargo excluded from CG/weight... ");

    Ship ship = {0};
    ship.length = 100.0f;
    ship.width = 20.0f;
    ship.max_weight = 10000.0f * 1000.0f;
    ship.lightship_weight = 1000.0f * 1000.0f;
    ship.lightship_kg = 5.0f;

    Cargo items[] = {
        {.id = "Placed",   .weight = 100.0f * 1000.0f, .dimensions = {10.0f, 4.0f, 4.0f}, .type = "x",
         .pos_x = 45.0f, .pos_y = 8.0f, .pos_z = -2.0f, .placed_w = 10.0f, .placed_h = 4.0f},
        {.id = "Unplaced", .weight = 999.0f * 1000.0f, .dimensions = {10.0f, 4.0f, 4.0f}, .type = "x",
         .pos_x = -1.0f, .pos_y = -1.0f, .pos_z = -1.0f, .placed_w = 0.0f, .placed_h = 0.0f},
    };
    ship.cargo = items;
    ship.cargo_count = 2;

    AnalysisResult r = perform_analysis(&ship);
    assert(r.placed_item_count == 1 && "only the placed item counts");
    assert(approx(r.total_cargo_weight_kg, 100000.0f, 1.0f));

    printf("OK\n");
}

/* A very high vertical center of gravity must drive GM negative (KB + BM < KG),
 * i.e. the simulator must flag a top-heavy ship as UNSTABLE rather than just low. */
static void test_top_heavy_load_is_unstable(void) {
    printf("  top-heavy ship -> negative GM (UNSTABLE)... ");

    Ship ship = {0};
    ship.length = 50.0f;
    ship.width = 10.0f;
    ship.max_weight = 10000.0f * 1000.0f;
    ship.lightship_weight = 1000.0f * 1000.0f;
    ship.lightship_kg = 15.0f;  /* lightship CG sits very high above the keel */

    Cargo item = {
        .id = "OnDeck", .weight = 10.0f * 1000.0f, .dimensions = {5.0f, 5.0f, 4.0f},
        .type = "general", .pos_x = 22.0f, .pos_y = 4.0f, .pos_z = 0.0f,  /* on deck */
        .placed_w = 5.0f, .placed_h = 5.0f,
    };
    ship.cargo = &item;
    ship.cargo_count = 1;

    AnalysisResult r = perform_analysis(&ship);
    assert(!isnan(r.gm) && "not overweight, so GM should be a real number");
    assert(r.gm < 0.0f && "a top-heavy ship must report negative (unstable) GM");

    printf("OK (GM = %.2f m)\n", (double)r.gm);
}

/* The hydrostatics must be self-consistent and physically sane: displacement =
 * lightship + cargo, deadweight = cargo, GM = KB + BM - KG, a centered load has
 * near-zero trim/heel, and freeboard = depth - draft when a depth is given. */
static void test_hydrostatics_are_consistent(void) {
    printf("  hydrostatics consistent (disp, GM=KB+BM-KG, freeboard)... ");

    Ship ship = {0};
    ship.length = 100.0f;
    ship.width = 20.0f;
    ship.depth = 12.0f;
    ship.max_weight = 10000.0f * 1000.0f;
    ship.lightship_weight = 1000.0f * 1000.0f;
    ship.lightship_kg = 5.0f;

    Cargo item = {
        .id = "Centered", .weight = 100.0f * 1000.0f, .dimensions = {10.0f, 4.0f, 4.0f},
        .type = "general", .pos_x = 45.0f, .pos_y = 8.0f, .pos_z = -2.0f,
        .placed_w = 10.0f, .placed_h = 4.0f,
    };
    ship.cargo = &item;
    ship.cargo_count = 1;

    AnalysisResult r = perform_analysis(&ship);

    assert(approx(r.displacement_t, 1100.0f, 0.5f) && "displacement = lightship + cargo");
    assert(approx(r.deadweight_t, 100.0f, 0.5f) && "deadweight = cargo weight");
    assert(approx(r.gm, r.kb + r.bm - r.kg, 0.01f) && "GM must equal KB + BM - KG");
    assert(r.draft_mean > 0.0f && r.volume_m3 > 0.0f);
    assert(approx(r.draft_fore, r.draft_aft, 0.05f) && "centered load => ~zero trim");
    assert(fabsf(r.heel_deg) < 1.0f && "centered load => ~upright");
    assert(approx(r.freeboard, ship.depth - r.draft_mean, 0.01f) && "freeboard = depth - draft");

    printf("OK\n");
}

/* The derived stability criteria must be self-consistent: margin = GM(fluid) - min,
 * GZ at 30° is positive for a stable ship, and a dry load has no free-surface loss. */
static void test_stability_criteria(void) {
    printf("  stability criteria (margin, GZ30, free-surface)... ");

    Ship ship = {0};
    ship.length = 100.0f;
    ship.width = 20.0f;
    ship.max_weight = 10000.0f * 1000.0f;
    ship.lightship_weight = 1000.0f * 1000.0f;
    ship.lightship_kg = 5.0f;

    Cargo item = {
        .id = "Dry", .weight = 100.0f * 1000.0f, .dimensions = {10.0f, 4.0f, 4.0f},
        .type = "general", .pos_x = 45.0f, .pos_y = 8.0f, .pos_z = -2.0f,
        .placed_w = 10.0f, .placed_h = 4.0f,
    };
    ship.cargo = &item;
    ship.cargo_count = 1;

    AnalysisResult r = perform_analysis(&ship);
    assert(r.fs_correction == 0.0f && "no liquid cargo => no free-surface loss");
    assert(approx(r.gm_fluid, r.gm, 0.001f) && "GM(fluid) == GM when no free surface");
    assert(approx(r.gm_margin, r.gm_fluid - MIN_GM, 0.001f));
    assert(r.gz30 > 0.0f && "a stable ship has positive GZ at 30 degrees");

    printf("OK\n");
}

int main(void) {
    printf("--- Running Analysis Tests ---\n");
    test_centered_load_is_balanced_and_stable();
    test_overweight_plan_is_rejected();
    test_unplaced_cargo_is_excluded();
    test_top_heavy_load_is_unstable();
    test_hydrostatics_are_consistent();
    test_stability_criteria();
    printf("--- All Analysis Tests Passed ---\n");
    return 0;
}
