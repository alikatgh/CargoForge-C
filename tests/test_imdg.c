/*
 * test_imdg.c - Tests for IMDG segregation engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "imdg.h"

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

static void test_class1_vs_all_incompatible(void) {
    printf("  test_class1_vs_all_incompatible...\n");
    /* Class 1.1-1.6 explosives should be incompatible with everything */
    for (int c = 1; c <= 9; c++) {
        SegregationType seg = imdg_get_segregation(1, 1, c, 0);
        ASSERT(seg == SEG_INCOMPATIBLE, "Class 1.1 vs everything = incompatible");
    }
}

static void test_class9_vs_all_none(void) {
    printf("  test_class9_vs_all_none...\n");
    /* Class 9 (misc) should have no segregation with most classes */
    for (int c = 2; c <= 9; c++) {
        SegregationType seg = imdg_get_segregation(9, 0, c, 0);
        ASSERT(seg == SEG_NONE, "Class 9 vs others = no segregation");
    }
}

static void test_matrix_symmetry(void) {
    printf("  test_matrix_symmetry...\n");
    /* The segregation matrix should be symmetric: A vs B == B vs A */
    int classes[] = {1,1, 1,7, 1,8, 2,1, 2,2, 2,3, 3,0, 4,1, 4,2, 4,3,
                     5,1, 5,2, 6,1, 6,2, 7,0, 8,0, 9,0};
    int num_classes = 17;

    for (int i = 0; i < num_classes; i++) {
        for (int j = i + 1; j < num_classes; j++) {
            int ca = classes[i*2], da = classes[i*2+1];
            int cb = classes[j*2], db = classes[j*2+1];
            SegregationType ab = imdg_get_segregation(ca, da, cb, db);
            SegregationType ba = imdg_get_segregation(cb, db, ca, da);
            if (ab != ba) {
                fprintf(stderr, "  FAIL: symmetry: %d.%d vs %d.%d: %d != %d\n",
                        ca, da, cb, db, ab, ba);
                tests_run++;
            } else {
                tests_run++;
                tests_passed++;
            }
        }
    }
}

static void test_known_segregation_pairs(void) {
    printf("  test_known_segregation_pairs...\n");

    /* Class 3 (flammable liquids) vs Class 5.1 (oxidizing): separated from (2) */
    SegregationType seg = imdg_get_segregation(3, 0, 5, 1);
    ASSERT(seg == SEG_SEPARATED, "Class 3 vs 5.1 = separated from");

    /* Class 2.1 (flam gas) vs Class 3 (flam liquid): separated from (2) */
    seg = imdg_get_segregation(2, 1, 3, 0);
    ASSERT(seg == SEG_SEPARATED, "Class 2.1 vs 3 = separated from");

    /* Class 6.2 (infectious) vs most = no segregation */
    seg = imdg_get_segregation(6, 2, 3, 0);
    ASSERT(seg == SEG_NONE, "Class 6.2 vs 3 = none");

    /* Class 4.2 (spont combust) vs Class 5.1 (oxidizing): separated from (2) */
    seg = imdg_get_segregation(4, 2, 5, 1);
    ASSERT(seg == SEG_SEPARATED, "Class 4.2 vs 5.1 = separated from");
}

static void test_min_distance(void) {
    printf("  test_min_distance...\n");
    ASSERT_NEAR(imdg_min_distance(SEG_NONE), 0.0f, 0.01f, "none = 0m");
    ASSERT_NEAR(imdg_min_distance(SEG_AWAY_FROM), 3.0f, 0.01f, "away from = 3m");
    ASSERT_NEAR(imdg_min_distance(SEG_SEPARATED), 6.0f, 0.01f, "separated = 6m");
    ASSERT_NEAR(imdg_min_distance(SEG_SEPARATED_COMPLETE), 12.0f, 0.01f, "complete = 12m");
    ASSERT_NEAR(imdg_min_distance(SEG_SEPARATED_LONG), 24.0f, 0.01f, "longitudinal = 24m");
    ASSERT_NEAR(imdg_min_distance(SEG_INCOMPATIBLE), -1.0f, 0.01f, "incompatible = -1");
}

static void test_segregation_names(void) {
    printf("  test_segregation_names...\n");
    ASSERT(strcmp(imdg_segregation_name(SEG_NONE), "None") == 0, "name: none");
    ASSERT(strcmp(imdg_segregation_name(SEG_AWAY_FROM), "Away from") == 0, "name: away from");
    ASSERT(strcmp(imdg_segregation_name(SEG_INCOMPATIBLE), "Incompatible") == 0, "name: incompatible");
}

static void test_imdg_check_all_compliant(void) {
    printf("  test_imdg_check_all_compliant...\n");

    /* Two DG cargo items far apart */
    Cargo cargo[2];
    memset(cargo, 0, sizeof(cargo));

    DGInfo dg1 = { .dg_class = 3, .dg_division = 1 }; /* flammable liquid */
    DGInfo dg2 = { .dg_class = 8, .dg_division = 0 }; /* corrosive */

    strcpy(cargo[0].id, "FlammLiq");
    cargo[0].weight = 25000.0f;
    cargo[0].dimensions[0] = 6.0f;
    cargo[0].dimensions[1] = 2.5f;
    cargo[0].dimensions[2] = 2.6f;
    cargo[0].pos_x = 10.0f;
    cargo[0].pos_y = 5.0f;
    cargo[0].pos_z = 0.0f;
    cargo[0].dg = &dg1;

    strcpy(cargo[1].id, "Corrosive");
    cargo[1].weight = 20000.0f;
    cargo[1].dimensions[0] = 6.0f;
    cargo[1].dimensions[1] = 2.5f;
    cargo[1].dimensions[2] = 2.6f;
    cargo[1].pos_x = 100.0f; /* far away */
    cargo[1].pos_y = 5.0f;
    cargo[1].pos_z = 0.0f;
    cargo[1].dg = &dg2;

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 150.0f;
    ship.width = 25.0f;
    ship.cargo = cargo;
    ship.cargo_count = 2;

    IMDGCheckResult result = imdg_check_all(&ship);
    /* Class 3 vs 8: away from (1), min distance 3m. Items are ~84m apart. */
    ASSERT(result.compliant == 1, "far apart DG cargo is compliant");
    ASSERT(result.violation_count == 0, "no violations");
}

static void test_imdg_check_all_violation(void) {
    printf("  test_imdg_check_all_violation...\n");

    /* Two DG cargo items too close */
    Cargo cargo[2];
    memset(cargo, 0, sizeof(cargo));

    DGInfo dg1 = { .dg_class = 3, .dg_division = 1 }; /* flammable liquid */
    DGInfo dg2 = { .dg_class = 5, .dg_division = 1 }; /* oxidizer */

    strcpy(cargo[0].id, "FlammLiq");
    cargo[0].weight = 25000.0f;
    cargo[0].dimensions[0] = 6.0f;
    cargo[0].dimensions[1] = 2.5f;
    cargo[0].dimensions[2] = 2.6f;
    cargo[0].pos_x = 10.0f;
    cargo[0].pos_y = 5.0f;
    cargo[0].pos_z = 0.0f;
    cargo[0].dg = &dg1;

    strcpy(cargo[1].id, "Oxidizer");
    cargo[1].weight = 15000.0f;
    cargo[1].dimensions[0] = 6.0f;
    cargo[1].dimensions[1] = 2.5f;
    cargo[1].dimensions[2] = 2.6f;
    cargo[1].pos_x = 17.0f; /* only 1m gap */
    cargo[1].pos_y = 5.0f;
    cargo[1].pos_z = 0.0f;
    cargo[1].dg = &dg2;

    Ship ship;
    memset(&ship, 0, sizeof(ship));
    ship.length = 150.0f;
    ship.width = 25.0f;
    ship.cargo = cargo;
    ship.cargo_count = 2;

    IMDGCheckResult result = imdg_check_all(&ship);
    /* Class 3 vs 5.1: separated from (2), min distance 6m. Items are ~1m apart. */
    ASSERT(result.compliant == 0, "close DG cargo has violations");
    ASSERT(result.violation_count > 0, "at least one violation");
}

int main(void) {
    printf("=== IMDG Segregation Tests ===\n");

    test_class1_vs_all_incompatible();
    test_class9_vs_all_none();
    test_matrix_symmetry();
    test_known_segregation_pairs();
    test_min_distance();
    test_segregation_names();
    test_imdg_check_all_compliant();
    test_imdg_check_all_violation();

    printf("IMDG: %d/%d tests passed\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
