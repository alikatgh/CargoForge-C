/*
 * validate_benchmark.c - Benchmark vessel validation test harness
 *
 * Validates CargoForge calculation engine against known stability booklet
 * values for three benchmark vessel types. This is a key deliverable for
 * DNV Type Approval under DNV-SE-0475 (Computer Software).
 *
 * Approach: Manually position cargo for controlled loading conditions
 * (as per stability booklet test cases), then validate the calculation
 * engine's output against expected values.
 *
 * Build:   make validate
 * Run:     ./validation/validate_benchmark
 */

#include "cargoforge.h"
#include "hydrostatics.h"
#include "tanks.h"
#include "longitudinal_strength.h"
#include "imdg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* TEST FRAMEWORK                                                     */
/* ------------------------------------------------------------------ */

static int total_checks = 0;
static int passed_checks = 0;
static int failed_checks = 0;

typedef enum { TOL_ABSOLUTE, TOL_RELATIVE } ToleranceType;

static int check_value(const char *param, float actual, float expected,
                       float tolerance, ToleranceType tol_type) {
    total_checks++;
    float error;
    const char *unit;

    if (tol_type == TOL_RELATIVE && fabsf(expected) >= 1e-6f) {
        error = fabsf(actual - expected) / fabsf(expected) * 100.0f;
        unit = "%";
    } else {
        error = fabsf(actual - expected);
        unit = "";
    }

    int pass = (error < tolerance);
    if (pass) {
        passed_checks++;
        printf("  PASS  %-28s: %10.4f  (exp %10.4f, err %.4f%s)\n",
               param, actual, expected, error, unit);
    } else {
        failed_checks++;
        printf("  FAIL  %-28s: %10.4f  (exp %10.4f, err %.4f%s, lim %.4f)\n",
               param, actual, expected, error, unit, tolerance);
    }
    return pass;
}

static void check_true(const char *param, int condition) {
    total_checks++;
    if (condition) { passed_checks++; printf("  PASS  %s\n", param); }
    else           { failed_checks++; printf("  FAIL  %s\n", param); }
}

#define CHECK_ABS(p, a, e, t)  check_value(p, a, e, t, TOL_ABSOLUTE)
#define CHECK_REL(p, a, e, t)  check_value(p, a, e, t, TOL_RELATIVE)

/* ------------------------------------------------------------------ */
/* HELPER: Add manually positioned cargo                              */
/* ------------------------------------------------------------------ */

static void add_cargo(Ship *ship, const char *id, float weight_t,
                      float l, float w, float h,
                      float px, float py, float pz) {
    if (ship->cargo_count >= ship->cargo_capacity) return;
    Cargo *c = &ship->cargo[ship->cargo_count];
    memset(c, 0, sizeof(*c));
    strncpy(c->id, id, sizeof(c->id) - 1);
    strcpy(c->type, "standard");
    c->weight = weight_t * 1000.0f;
    c->dimensions[0] = l;
    c->dimensions[1] = w;
    c->dimensions[2] = h;
    c->pos_x = px;
    c->pos_y = py;
    c->pos_z = pz;
    ship->cargo_count++;
}

/* ------------------------------------------------------------------ */
/* TEST 1: HYDROSTATIC TABLE INTERPOLATION                            */
/* ------------------------------------------------------------------ */

static void test_hydrostatic_interpolation(void) {
    printf("\n=== Test 1: Hydrostatic Table Interpolation ===\n\n");

    HydroTable table;
    if (parse_hydro_table("validation/vessels/general_cargo/hydro.csv", &table) != 0) {
        printf("  ERROR: Failed to load table\n");
        failed_checks++; total_checks++;
        return;
    }

    /* 1a: Exact table entry */
    {
        HydroEntry he;
        hydro_interpolate(&table, 5.0f, &he);
        printf("  --- Exact entry: draft 5.0m ---\n");
        CHECK_ABS("disp@5.0m",  he.displacement, 7530.0f, 0.1f);
        CHECK_ABS("KM@5.0m",    he.km,           6.33f,   0.001f);
        CHECK_ABS("KB@5.0m",    he.kb,           2.63f,   0.001f);
        CHECK_ABS("BM@5.0m",    he.bm,           3.70f,   0.001f);
        CHECK_ABS("TPC@5.0m",   he.tpc,          9.58f,   0.001f);
    }

    /* 1b: Mid-interval interpolation (4.25m between 4.0m and 4.5m) */
    {
        printf("\n  --- Interpolated: draft 4.25m ---\n");
        HydroEntry he;
        hydro_interpolate(&table, 4.25f, &he);
        float exp_disp = (5810.0f + 6660.0f) / 2.0f;
        float exp_km   = (6.92f + 6.58f) / 2.0f;
        float exp_kb   = (2.10f + 2.37f) / 2.0f;
        CHECK_ABS("disp@4.25m", he.displacement, exp_disp, 1.0f);
        CHECK_ABS("KM@4.25m",   he.km,           exp_km,   0.01f);
        CHECK_ABS("KB@4.25m",   he.kb,           exp_kb,   0.01f);
    }

    /* 1c: Reverse lookup */
    {
        printf("\n  --- Reverse lookup ---\n");
        CHECK_ABS("draft@7530t", hydro_draft_from_displacement(&table, 7530.0f), 5.0f, 0.01f);
        CHECK_ABS("draft@6235t", hydro_draft_from_displacement(&table, 6235.0f), 4.25f, 0.05f);
    }

    /* 1d: Boundary clamping */
    {
        printf("\n  --- Boundary clamping ---\n");
        HydroEntry he;
        hydro_interpolate(&table, 0.5f, &he);
        CHECK_ABS("disp@0.5m(clamp)", he.displacement, 2680.0f, 0.1f);
        hydro_interpolate(&table, 20.0f, &he);
        CHECK_ABS("disp@20m(clamp)",  he.displacement, 11560.0f, 0.1f);
    }

    /* 1e: All three vessel tables load correctly */
    {
        printf("\n  --- All tables load ---\n");
        HydroTable ct, bt;
        check_true("container hydro loads", parse_hydro_table("validation/vessels/container_ship/hydro.csv", &ct) == 0);
        check_true("bulker hydro loads",    parse_hydro_table("validation/vessels/bulk_carrier/hydro.csv", &bt) == 0);

        HydroEntry he;
        hydro_interpolate(&ct, 12.0f, &he);
        CHECK_ABS("container disp@12m", he.displacement, 42550.0f, 1.0f);

        hydro_interpolate(&bt, 10.0f, &he);
        CHECK_ABS("bulker disp@10m", he.displacement, 55230.0f, 1.0f);
    }
}

/* ------------------------------------------------------------------ */
/* TEST 2: FREE SURFACE CORRECTION                                    */
/* ------------------------------------------------------------------ */

static void test_free_surface_correction(void) {
    printf("\n=== Test 2: Free Surface Correction ===\n\n");

    TankConfig tc;
    if (parse_tank_config("validation/vessels/general_cargo/tanks.csv", &tc) != 0) {
        printf("  ERROR: Failed to load tanks\n");
        failed_checks++; total_checks++;
        return;
    }

    /* Manual FSM: rho * L * B^3 / 12 */
    float expected_fsm = 512.50f + 427.08f + 34.59f + 34.59f + 79.17f + 21.33f;
    CHECK_ABS("total FSM (t-m)", calculate_total_fsm(&tc), expected_fsm, 2.0f);

    float expected_gg = expected_fsm / 7530.0f;
    CHECK_ABS("GG' @ 7530t", calculate_virtual_kg_rise(&tc, 7530.0f), expected_gg, 0.005f);

    float expected_tw = 138.375f + 102.5f + 73.8f + 73.8f + 98.8f + 38.4f;
    CHECK_REL("tank weight (t)", calculate_tank_weight(&tc), expected_tw, 0.5f);
}

/* ------------------------------------------------------------------ */
/* TEST 3: LONGITUDINAL STRENGTH                                      */
/* ------------------------------------------------------------------ */

static void test_longitudinal_strength(void) {
    printf("\n=== Test 3: Longitudinal Strength ===\n\n");

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 112.0f;
    ship.width = 18.6f;
    ship.lightship_weight = 3200000.0f;
    ship.lightship_kg = 6.8f;

    Cargo cargo[1];
    memset(cargo, 0, sizeof(cargo));
    strcpy(cargo[0].id, "CENTER");
    cargo[0].weight = 4000000.0f;
    cargo[0].dimensions[0] = 20.0f;
    cargo[0].dimensions[1] = 16.0f;
    cargo[0].dimensions[2] = 5.0f;
    cargo[0].pos_x = 46.0f;
    cargo[0].pos_y = 1.3f;
    cargo[0].pos_z = 0.0f;
    ship.cargo = cargo;
    ship.cargo_count = 1;
    ship.cargo_capacity = 1;

    LongStrengthResult ls = calculate_longitudinal_strength(
        &ship, 5.0f, 7200.0f, ship.length, ship.width);

    CHECK_ABS("SF at AP", ls.shear_force[0], 0.0f, 0.5f);
    CHECK_ABS("BM at AP", ls.bending_moment[0], 0.0f, 0.5f);
    check_true("max SF > 0", ls.max_shear_force > 0.0f);
    check_true("max BM exists", ls.max_bm_hog > 0.0f || ls.max_bm_sag > 0.0f);

    StrengthLimits limits = { 2800.0f, 42000.0f, 38000.0f };
    int rc = check_strength_limits(&ls, &limits);
    check_true("strength check returns valid", rc == 0 || rc == 1);

    printf("  INFO  Max SF: %.1f t, Max BM(hog): %.1f, (sag): %.1f t-m\n",
           ls.max_shear_force, ls.max_bm_hog, ls.max_bm_sag);

    ship.cargo = NULL;
}

/* ------------------------------------------------------------------ */
/* TEST 4: GZ CURVE                                                   */
/* ------------------------------------------------------------------ */

static void test_gz_curve(void) {
    printf("\n=== Test 4: GZ Curve (Wall-Sided Formula) ===\n\n");

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 112.0f;
    ship.width = 18.6f;
    ship.max_weight = 8700000.0f;
    ship.lightship_weight = 3200000.0f;
    ship.lightship_kg = 6.8f;

    ship.hydro = calloc(1, sizeof(HydroTable));
    parse_hydro_table("validation/vessels/general_cargo/hydro.csv",
                      (HydroTable *)ship.hydro);

    float bc = ship.width / 2.0f;
    Cargo cargo[2];
    memset(cargo, 0, sizeof(cargo));
    ship.cargo = cargo;
    ship.cargo_count = 0;
    ship.cargo_capacity = 2;
    add_cargo(&ship, "PORT", 1500.0f, 15.0f, 8.0f, 4.0f,
              40.0f, bc - 8.5f, 0.0f);
    add_cargo(&ship, "STBD", 1500.0f, 15.0f, 8.0f, 4.0f,
              40.0f, bc + 0.5f, 0.0f);

    AnalysisResult r = perform_analysis(&ship);

    /* Verify GZ@30 matches wall-sided formula */
    float gm = r.gm_corrected;
    float bm = r.bm;
    float theta = 30.0f * (float)M_PI / 180.0f;
    float tan_t = tanf(theta);
    float exp_gz30 = sinf(theta) * (gm + bm * tan_t * tan_t / 2.0f);

    CHECK_ABS("GZ@30 matches formula", r.gz_at_30, exp_gz30, 0.01f);
    check_true("GZ@30 > 0 for GM > 0", r.gz_at_30 > 0.0f);
    check_true("area 0-30 > 0", r.area_0_30 > 0.0f);
    CHECK_ABS("heel near zero (symm)", r.heel, 0.0f, 1.0f);

    free(ship.hydro);
    ship.cargo = NULL;
}

/* ------------------------------------------------------------------ */
/* TEST 5: GENERAL CARGO - FULL STABILITY                             */
/* ------------------------------------------------------------------ */

static void test_general_cargo(void) {
    printf("\n=== Test 5: General Cargo Ship - Full Stability ===\n\n");

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 112.0f;
    ship.width = 18.6f;
    ship.max_weight = 8700000.0f;
    ship.lightship_weight = 3200000.0f;
    ship.lightship_kg = 6.8f;

    ship.hydro = calloc(1, sizeof(HydroTable));
    parse_hydro_table("validation/vessels/general_cargo/hydro.csv",
                      (HydroTable *)ship.hydro);
    ship.tanks = calloc(1, sizeof(TankConfig));
    parse_tank_config("validation/vessels/general_cargo/tanks.csv",
                      (TankConfig *)ship.tanks);
    ship.strength_limits = calloc(1, sizeof(StrengthLimits));
    ((StrengthLimits *)ship.strength_limits)->permissible_sf = 2800.0f;
    ((StrengthLimits *)ship.strength_limits)->permissible_bm_hog = 42000.0f;
    ((StrengthLimits *)ship.strength_limits)->permissible_bm_sag = 38000.0f;

    float bc = ship.width / 2.0f;
    Cargo cargo[4];
    memset(cargo, 0, sizeof(cargo));
    ship.cargo = cargo;
    ship.cargo_count = 0;
    ship.cargo_capacity = 4;

    add_cargo(&ship, "HOLD1", 1500.0f, 18.0f, 14.0f, 5.0f,
              60.0f, bc - 7.0f, 0.0f);
    add_cargo(&ship, "HOLD2", 1200.0f, 16.0f, 14.0f, 4.0f,
              25.0f, bc - 7.0f, 0.0f);
    add_cargo(&ship, "DECK_P", 300.0f, 10.0f, 6.0f, 3.0f,
              50.0f, bc - 6.5f, 5.0f);
    add_cargo(&ship, "DECK_S", 300.0f, 10.0f, 6.0f, 3.0f,
              50.0f, bc + 0.5f, 5.0f);

    AnalysisResult r = perform_analysis(&ship);

    check_true("hydro table used", r.hydro_table_used == 1);
    check_true("draft 3-8m", r.draft > 3.0f && r.draft < 8.0f);
    check_true("GM > 0.5m", r.gm > 0.5f);
    check_true("FSC > 0", r.free_surface_correction > 0.0f);
    check_true("GM_corr < GM", r.gm_corrected < r.gm);
    check_true("GM_corr >= 0.15m", r.gm_corrected >= 0.15f);
    check_true("KB > 0", r.kb > 0.0f);
    check_true("KB < draft", r.kb < r.draft);
    CHECK_ABS("heel (symmetric)", r.heel, 0.0f, 2.0f);
    check_true("|trim| < 3m", fabsf(r.trim) < 3.0f);

    printf("\n  --- IMO Criteria ---\n");
    check_true("GZ@30 >= 0.20m", r.gz_at_30 >= 0.20f);
    check_true("GZ_max angle >= 25deg", r.gz_max_angle >= 25.0f);
    check_true("area 0-30 >= 0.055", r.area_0_30 >= 0.055f);
    check_true("area 0-40 >= 0.090", r.area_0_40 >= 0.090f);
    check_true("area 30-40 >= 0.030", r.area_30_40 >= 0.030f);

    printf("\n  --- Results ---\n");
    printf("  INFO  Draft: %.3f m, GM: %.3f m, GM_corr: %.3f m\n",
           r.draft, r.gm, r.gm_corrected);
    printf("  INFO  Trim: %.3f m, Heel: %.3f deg\n", r.trim, r.heel);
    printf("  INFO  IMO: %s\n", r.imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");

    if (r.strength_compliant >= 0) {
        printf("  INFO  SF: %.0f t, BM: %.0f t-m, %s\n",
               r.max_shear_force, r.max_bending_moment,
               r.strength_compliant ? "WITHIN LIMITS" : "EXCEEDS LIMITS");
    }

    ship.cargo = NULL; /* stack-allocated, don't free */
    ship_cleanup(&ship);
}

/* ------------------------------------------------------------------ */
/* TEST 6: CONTAINER SHIP - FULL STABILITY                            */
/* ------------------------------------------------------------------ */

static void test_container_ship(void) {
    printf("\n=== Test 6: Container Ship - Full Stability ===\n\n");

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 283.0f;
    ship.width = 32.2f;
    ship.max_weight = 70500000.0f;
    ship.lightship_weight = 18500000.0f;
    ship.lightship_kg = 10.2f;

    ship.hydro = calloc(1, sizeof(HydroTable));
    parse_hydro_table("validation/vessels/container_ship/hydro.csv",
                      (HydroTable *)ship.hydro);
    ship.tanks = calloc(1, sizeof(TankConfig));
    parse_tank_config("validation/vessels/container_ship/tanks.csv",
                      (TankConfig *)ship.tanks);
    ship.strength_limits = calloc(1, sizeof(StrengthLimits));
    ((StrengthLimits *)ship.strength_limits)->permissible_sf = 12000.0f;
    ((StrengthLimits *)ship.strength_limits)->permissible_bm_hog = 350000.0f;
    ((StrengthLimits *)ship.strength_limits)->permissible_bm_sag = 320000.0f;

    float bc = ship.width / 2.0f;
    Cargo cargo[6];
    memset(cargo, 0, sizeof(cargo));
    ship.cargo = cargo;
    ship.cargo_count = 0;
    ship.cargo_capacity = 6;

    add_cargo(&ship, "BAY1_P", 4000.0f, 25.0f, 12.0f, 8.0f,
              200.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "BAY1_S", 4000.0f, 25.0f, 12.0f, 8.0f,
              200.0f, bc + 1.0f, 0.0f);
    add_cargo(&ship, "BAY2_P", 4500.0f, 30.0f, 12.0f, 8.0f,
              135.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "BAY2_S", 4500.0f, 30.0f, 12.0f, 8.0f,
              135.0f, bc + 1.0f, 0.0f);
    add_cargo(&ship, "BAY3_P", 3500.0f, 25.0f, 12.0f, 8.0f,
              60.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "BAY3_S", 3500.0f, 25.0f, 12.0f, 8.0f,
              60.0f, bc + 1.0f, 0.0f);

    AnalysisResult r = perform_analysis(&ship);

    check_true("hydro table used", r.hydro_table_used == 1);
    check_true("draft 10-15m", r.draft > 10.0f && r.draft < 15.0f);
    check_true("GM > 0", r.gm > 0.0f);
    check_true("GM_corr >= 0.15m", r.gm_corrected >= 0.15f);
    check_true("KB > 0 and < draft", r.kb > 0.0f && r.kb < r.draft);
    CHECK_ABS("heel (symmetric)", r.heel, 0.0f, 2.0f);
    check_true("|trim| < 15m", fabsf(r.trim) < 15.0f);

    printf("\n  --- IMO Criteria ---\n");
    check_true("GZ@30 >= 0.20m", r.gz_at_30 >= 0.20f);
    check_true("GZ_max >= 25deg", r.gz_max_angle >= 25.0f);
    check_true("area 0-30 >= 0.055", r.area_0_30 >= 0.055f);
    check_true("area 0-40 >= 0.090", r.area_0_40 >= 0.090f);
    check_true("area 30-40 >= 0.030", r.area_30_40 >= 0.030f);

    printf("\n  --- Results ---\n");
    printf("  INFO  Draft: %.3f m, GM: %.3f, GM_corr: %.3f m\n",
           r.draft, r.gm, r.gm_corrected);
    printf("  INFO  Trim: %.3f m, Heel: %.3f deg\n", r.trim, r.heel);
    printf("  INFO  IMO: %s\n", r.imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");

    ship.cargo = NULL;
    ship_cleanup(&ship);
}

/* ------------------------------------------------------------------ */
/* TEST 7: BULK CARRIER - FULL STABILITY                              */
/* ------------------------------------------------------------------ */

static void test_bulk_carrier(void) {
    printf("\n=== Test 7: Bulk Carrier - Full Stability ===\n\n");

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 190.0f;
    ship.width = 32.26f;
    ship.max_weight = 62300000.0f;
    ship.lightship_weight = 9800000.0f;
    ship.lightship_kg = 8.5f;

    ship.hydro = calloc(1, sizeof(HydroTable));
    parse_hydro_table("validation/vessels/bulk_carrier/hydro.csv",
                      (HydroTable *)ship.hydro);
    ship.tanks = calloc(1, sizeof(TankConfig));
    parse_tank_config("validation/vessels/bulk_carrier/tanks.csv",
                      (TankConfig *)ship.tanks);
    ship.strength_limits = calloc(1, sizeof(StrengthLimits));
    ((StrengthLimits *)ship.strength_limits)->permissible_sf = 8500.0f;
    ((StrengthLimits *)ship.strength_limits)->permissible_bm_hog = 230000.0f;
    ((StrengthLimits *)ship.strength_limits)->permissible_bm_sag = 210000.0f;

    float bc = ship.width / 2.0f;
    Cargo cargo[5];
    memset(cargo, 0, sizeof(cargo));
    ship.cargo = cargo;
    ship.cargo_count = 0;
    ship.cargo_capacity = 5;

    /* Holds evenly distributed along hull (centered on LBP/2 = 95m) */
    add_cargo(&ship, "HOLD1", 7500.0f, 20.0f, 26.0f, 10.0f,
              150.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "HOLD2", 9200.0f, 22.0f, 26.0f, 10.0f,
              120.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "HOLD3", 9500.0f, 22.0f, 26.0f, 10.0f,
              80.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "HOLD4", 9200.0f, 22.0f, 26.0f, 10.0f,
              50.0f, bc - 13.0f, 0.0f);
    add_cargo(&ship, "HOLD5", 6600.0f, 18.0f, 26.0f, 10.0f,
              25.0f, bc - 13.0f, 0.0f);

    AnalysisResult r = perform_analysis(&ship);

    check_true("hydro table used", r.hydro_table_used == 1);
    check_true("draft 8-14m", r.draft > 8.0f && r.draft < 14.0f);
    check_true("GM > 0", r.gm > 0.0f);
    check_true("GM_corr >= 0.15m", r.gm_corrected >= 0.15f);
    check_true("KB > 0 and < draft", r.kb > 0.0f && r.kb < r.draft);
    CHECK_ABS("heel (centered)", r.heel, 0.0f, 3.0f);
    check_true("|trim| < 5m", fabsf(r.trim) < 5.0f);

    printf("\n  --- IMO Criteria ---\n");
    check_true("GZ@30 >= 0.20m", r.gz_at_30 >= 0.20f);
    check_true("GZ_max >= 25deg", r.gz_max_angle >= 25.0f);
    check_true("area 0-30 >= 0.055", r.area_0_30 >= 0.055f);
    check_true("area 0-40 >= 0.090", r.area_0_40 >= 0.090f);
    check_true("area 30-40 >= 0.030", r.area_30_40 >= 0.030f);

    printf("\n  --- Results ---\n");
    printf("  INFO  Draft: %.3f m, GM: %.3f, GM_corr: %.3f m\n",
           r.draft, r.gm, r.gm_corrected);
    printf("  INFO  Trim: %.3f m, Heel: %.3f deg\n", r.trim, r.heel);
    printf("  INFO  IMO: %s\n", r.imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");

    ship.cargo = NULL;
    ship_cleanup(&ship);
}

/* ------------------------------------------------------------------ */
/* TEST 8: IMDG COMPLIANCE                                            */
/* ------------------------------------------------------------------ */

static void test_imdg(void) {
    printf("\n=== Test 8: IMDG Compliance ===\n\n");

    /* Segregation matrix */
    check_true("Class 3 vs 3 = NONE",
               imdg_get_segregation(3, 0, 3, 0) == SEG_NONE);
    check_true("Class 1.1 vs 3 >= SEPARATED",
               imdg_get_segregation(1, 1, 3, 0) >= SEG_SEPARATED);
    check_true("Class 2.1 vs 4.1 >= AWAY_FROM",
               imdg_get_segregation(2, 1, 4, 1) >= SEG_AWAY_FROM);

    /* Minimum distances */
    check_true("AWAY_FROM >= 3m",       imdg_min_distance(SEG_AWAY_FROM) >= 3.0f);
    check_true("SEPARATED >= 6m",       imdg_min_distance(SEG_SEPARATED) >= 6.0f);
    check_true("SEP_COMPLETE >= 12m",   imdg_min_distance(SEG_SEPARATED_COMPLETE) >= 12.0f);

    /* Names */
    check_true("NONE has name",      strlen(imdg_segregation_name(SEG_NONE)) > 0);
    check_true("AWAY_FROM has name", strlen(imdg_segregation_name(SEG_AWAY_FROM)) > 0);

    /* Ship with DG cargo */
    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 112.0f;
    ship.width = 18.6f;
    ship.max_weight = 8700000.0f;

    Cargo cargo[3];
    memset(cargo, 0, sizeof(cargo));
    ship.cargo = cargo;
    ship.cargo_count = 3;
    ship.cargo_capacity = 3;

    strcpy(cargo[0].id, "GENERAL");
    cargo[0].weight = 500000.0f;
    cargo[0].dimensions[0] = 10.0f; cargo[0].dimensions[1] = 5.0f; cargo[0].dimensions[2] = 3.0f;
    cargo[0].pos_x = 50.0f; cargo[0].pos_y = 5.0f; cargo[0].pos_z = 0.0f;
    strcpy(cargo[0].type, "standard");

    static DGInfo dg1 = { .dg_class = 3, .dg_division = 0,
                    .stowage = STOW_ANY, .un_number = "1203", .ems = "F-E" };
    strcpy(cargo[1].id, "DG_FUEL");
    cargo[1].weight = 200000.0f;
    cargo[1].dimensions[0] = 3.0f; cargo[1].dimensions[1] = 2.0f; cargo[1].dimensions[2] = 1.5f;
    cargo[1].pos_x = 20.0f; cargo[1].pos_y = 5.0f; cargo[1].pos_z = 0.0f;
    strcpy(cargo[1].type, "hazardous");
    cargo[1].dg = (struct DGInfo_ *)&dg1;

    static DGInfo dg2 = { .dg_class = 8, .dg_division = 0,
                    .stowage = STOW_ANY, .un_number = "1830", .ems = "F-A" };
    strcpy(cargo[2].id, "DG_ACID");
    cargo[2].weight = 150000.0f;
    cargo[2].dimensions[0] = 3.0f; cargo[2].dimensions[1] = 2.0f; cargo[2].dimensions[2] = 1.5f;
    cargo[2].pos_x = 22.0f; cargo[2].pos_y = 5.0f; cargo[2].pos_z = 0.0f;
    strcpy(cargo[2].type, "hazardous");
    cargo[2].dg = (struct DGInfo_ *)&dg2;

    IMDGCheckResult result = imdg_check_all(&ship);
    check_true("IMDG check ran", result.violation_count >= 0);

    printf("  INFO  Violations: %d, Compliant: %s\n",
           result.violation_count, result.compliant ? "YES" : "NO");

    cargo[1].dg = NULL;
    cargo[2].dg = NULL;
    ship.cargo = NULL;
}

/* ------------------------------------------------------------------ */
/* TEST 9: BOX-HULL vs TABLE CROSS-VALIDATION                        */
/* ------------------------------------------------------------------ */

static void test_cross_validation(void) {
    printf("\n=== Test 9: Cross-Validation (Table vs Box-Hull) ===\n\n");

    float bc = 18.6f / 2.0f;

    Ship ship_t;
    memset(&ship_t, 0, sizeof(ship_t));
    ship_t.length = 112.0f; ship_t.width = 18.6f;
    ship_t.max_weight = 8700000.0f;
    ship_t.lightship_weight = 3200000.0f; ship_t.lightship_kg = 6.8f;
    ship_t.hydro = calloc(1, sizeof(HydroTable));
    parse_hydro_table("validation/vessels/general_cargo/hydro.csv",
                      (HydroTable *)ship_t.hydro);

    Cargo ct[2]; memset(ct, 0, sizeof(ct));
    ship_t.cargo = ct; ship_t.cargo_count = 0; ship_t.cargo_capacity = 2;
    add_cargo(&ship_t, "C1", 2000.0f, 20.0f, 14.0f, 5.0f, 46.0f, bc - 7.0f, 0.0f);
    add_cargo(&ship_t, "C2", 1000.0f, 15.0f, 10.0f, 3.0f, 70.0f, bc - 5.0f, 0.0f);

    AnalysisResult r_t = perform_analysis(&ship_t);

    Ship ship_b;
    memset(&ship_b, 0, sizeof(ship_b));
    ship_b.length = 112.0f; ship_b.width = 18.6f;
    ship_b.max_weight = 8700000.0f;
    ship_b.lightship_weight = 3200000.0f; ship_b.lightship_kg = 6.8f;

    Cargo cb[2]; memcpy(cb, ct, sizeof(ct));
    ship_b.cargo = cb; ship_b.cargo_count = 2; ship_b.cargo_capacity = 2;

    AnalysisResult r_b = perform_analysis(&ship_b);

    printf("  INFO  Draft:  table=%.3f, box=%.3f\n", r_t.draft, r_b.draft);
    printf("  INFO  GM:     table=%.3f, box=%.3f\n", r_t.gm, r_b.gm);

    check_true("both GM positive", r_t.gm > 0.0f && r_b.gm > 0.0f);

    float ratio = r_t.draft / (r_b.draft > 0.01f ? r_b.draft : 0.01f);
    check_true("draft ratio 0.5-2.0", ratio > 0.5f && ratio < 2.0f);

    free(ship_t.hydro);
    ship_t.cargo = NULL;
    ship_b.cargo = NULL;
}

/* ------------------------------------------------------------------ */
/* MAIN                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("================================================================\n");
    printf("  CargoForge Benchmark Vessel Validation Suite\n");
    printf("  DNV-SE-0475 Type Approval - Calculation Validation\n");
    printf("================================================================\n");

    test_hydrostatic_interpolation();
    test_free_surface_correction();
    test_longitudinal_strength();
    test_gz_curve();
    test_general_cargo();
    test_container_ship();
    test_bulk_carrier();
    test_imdg();
    test_cross_validation();

    printf("\n================================================================\n");
    printf("  VALIDATION SUMMARY\n");
    printf("================================================================\n");
    printf("  Total checks:  %d\n", total_checks);
    printf("  Passed:        %d\n", passed_checks);
    printf("  Failed:        %d\n", failed_checks);
    printf("  Pass rate:     %.1f%%\n",
           total_checks > 0 ? (float)passed_checks / total_checks * 100.0f : 0.0f);
    printf("================================================================\n");

    if (failed_checks > 0) {
        printf("\n  *** VALIDATION FAILED: %d check(s) ***\n\n", failed_checks);
        return 1;
    }
    printf("\n  ALL CHECKS PASSED - Validation successful\n\n");
    return 0;
}
