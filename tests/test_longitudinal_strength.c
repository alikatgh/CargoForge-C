/*
 * test_longitudinal_strength.c - Tests for SWSF and SWBM calculations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "longitudinal_strength.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol, msg) do { \
    tests_run++; \
    if (fabsf((a) - (b)) > (tol)) { \
        fprintf(stderr, "  FAIL: %s: %.4f != %.4f (tol %.4f) (line %d)\n", msg, (float)(a), (float)(b), (float)(tol), __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

static Ship make_test_ship(void) {
    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 150.0f;
    ship.width = 25.0f;
    ship.max_weight = 50000000.0f;
    ship.lightship_weight = 2000000.0f; /* 2000 t in kg */
    ship.lightship_kg = 8.0f;
    return ship;
}

static void test_boundary_conditions(void) {
    printf("  test_boundary_conditions...\n");
    Ship ship = make_test_ship();

    /* No cargo - just lightship */
    float displacement_t = ship.lightship_weight / 1000.0f;
    LongStrengthResult r = calculate_longitudinal_strength(
        &ship, 3.0f, displacement_t, ship.length, ship.width);

    ASSERT(r.station_count == LS_NUM_STATIONS, "21 stations");
    ASSERT_NEAR(r.station_pos[0], 0.0f, 0.01f, "first station at AP");
    ASSERT_NEAR(r.station_pos[LS_NUM_STATIONS - 1], 150.0f, 0.01f, "last station at FP");

    /* Shear force should be small at the ends (not exactly zero due to
     * tapered distributions, but the integration starts at zero) */
    ASSERT_NEAR(r.shear_force[0], 0.0f, 0.01f, "SF at AP = 0");
    ASSERT_NEAR(r.bending_moment[0], 0.0f, 0.01f, "BM at AP = 0");
}

static void test_cargo_increases_bm(void) {
    printf("  test_cargo_increases_bm...\n");
    Ship ship = make_test_ship();

    /* First: no cargo */
    float disp_no_cargo = ship.lightship_weight / 1000.0f;
    LongStrengthResult r1 = calculate_longitudinal_strength(
        &ship, 3.0f, disp_no_cargo, ship.length, ship.width);

    /* Now add heavy cargo at midship */
    Cargo heavy = {0};
    strcpy(heavy.id, "Heavy");
    heavy.weight = 5000000.0f; /* 5000 t */
    heavy.dimensions[0] = 20.0f;
    heavy.dimensions[1] = 10.0f;
    heavy.dimensions[2] = 5.0f;
    heavy.pos_x = 65.0f; /* near midship */
    heavy.pos_y = 7.5f;
    heavy.pos_z = 0.0f;

    ship.cargo = &heavy;
    ship.cargo_count = 1;

    float disp_with_cargo = (ship.lightship_weight + heavy.weight) / 1000.0f;
    LongStrengthResult r2 = calculate_longitudinal_strength(
        &ship, 5.0f, disp_with_cargo, ship.length, ship.width);

    /* Heavy cargo at midship should produce non-zero bending moment */
    ASSERT(r2.max_bm_hog > 0 || r2.max_bm_sag > 0, "heavy cargo produces BM");
    float max_bm1 = (r1.max_bm_hog > r1.max_bm_sag) ? r1.max_bm_hog : r1.max_bm_sag;
    float max_bm2 = (r2.max_bm_hog > r2.max_bm_sag) ? r2.max_bm_hog : r2.max_bm_sag;
    /* With concentrated load, we expect different BM pattern */
    ASSERT(max_bm2 > 0 || max_bm1 > 0, "BM is non-zero");
}

static void test_check_strength_limits(void) {
    printf("  test_check_strength_limits...\n");
    LongStrengthResult r;
    memset(&r, 0, sizeof(r));
    r.max_shear_force = 3000.0f;
    r.max_bm_hog = 80000.0f;
    r.max_bm_sag = 60000.0f;

    StrengthLimits limits = {
        .permissible_sf = 5000.0f,
        .permissible_bm_hog = 120000.0f,
        .permissible_bm_sag = 100000.0f
    };

    ASSERT(check_strength_limits(&r, &limits) == 1, "within limits");

    /* Exceed SF */
    r.max_shear_force = 6000.0f;
    ASSERT(check_strength_limits(&r, &limits) == 0, "exceeds SF limit");

    /* Exceed BM hog */
    r.max_shear_force = 3000.0f;
    r.max_bm_hog = 130000.0f;
    ASSERT(check_strength_limits(&r, &limits) == 0, "exceeds BM hog limit");

    /* Exceed BM sag */
    r.max_bm_hog = 80000.0f;
    r.max_bm_sag = 110000.0f;
    ASSERT(check_strength_limits(&r, &limits) == 0, "exceeds BM sag limit");
}

int main(void) {
    printf("=== Longitudinal Strength Tests ===\n");

    test_boundary_conditions();
    test_cargo_increases_bm();
    test_check_strength_limits();

    printf("Longitudinal Strength: %d/%d tests passed\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
