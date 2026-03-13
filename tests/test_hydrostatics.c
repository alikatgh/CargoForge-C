/*
 * test_hydrostatics.c - Tests for hydrostatic table parsing and interpolation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hydrostatics.h"

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

/* Helper: write a test CSV file */
static const char *write_test_csv(void) {
    static const char *path = "/tmp/test_hydro.csv";
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    fprintf(fp, "# Test hydrostatic table\n");
    fprintf(fp, "2.0,2000,10.00,1.00,9.00,8.00,180.0,3000,1.0\n");
    fprintf(fp, "4.0,4000,8.00,2.00,6.00,10.00,200.0,3500,0.5\n");
    fprintf(fp, "6.0,6000,7.00,3.00,4.00,11.00,220.0,3800,0.0\n");
    fprintf(fp, "8.0,8000,6.50,4.00,2.50,12.00,240.0,4000,-0.5\n");
    fclose(fp);
    return path;
}

static void test_parse_valid(void) {
    printf("  test_parse_valid...\n");
    HydroTable table;
    const char *path = write_test_csv();
    ASSERT(path != NULL, "write test file");

    int ret = parse_hydro_table(path, &table);
    ASSERT(ret == 0, "parse returns 0");
    ASSERT(table.count == 4, "4 entries parsed");
    ASSERT(table.loaded == 1, "table marked as loaded");
    ASSERT_NEAR(table.entries[0].draft, 2.0f, 0.01f, "first draft");
    ASSERT_NEAR(table.entries[3].draft, 8.0f, 0.01f, "last draft");
    ASSERT_NEAR(table.entries[1].displacement, 4000.0f, 0.01f, "displacement at 4m");
}

static void test_parse_reject_unordered(void) {
    printf("  test_parse_reject_unordered...\n");
    const char *path = "/tmp/test_hydro_bad.csv";
    FILE *fp = fopen(path, "w");
    fprintf(fp, "4.0,4000,8.0,2.0,6.0,10.0,200.0,3500,0.5\n");
    fprintf(fp, "2.0,2000,10.0,1.0,9.0,8.0,180.0,3000,1.0\n");
    fclose(fp);

    HydroTable table;
    int ret = parse_hydro_table(path, &table);
    ASSERT(ret == -1, "reject non-ascending draft");
}

static void test_parse_reject_too_few(void) {
    printf("  test_parse_reject_too_few...\n");
    const char *path = "/tmp/test_hydro_one.csv";
    FILE *fp = fopen(path, "w");
    fprintf(fp, "2.0,2000,10.0,1.0,9.0,8.0,180.0,3000,1.0\n");
    fclose(fp);

    HydroTable table;
    int ret = parse_hydro_table(path, &table);
    ASSERT(ret == -1, "reject single-entry table");
}

static void test_interpolate_exact(void) {
    printf("  test_interpolate_exact...\n");
    HydroTable table;
    parse_hydro_table(write_test_csv(), &table);

    HydroEntry result;
    int ret = hydro_interpolate(&table, 4.0f, &result);
    ASSERT(ret == 0, "interpolate returns 0");
    ASSERT_NEAR(result.draft, 4.0f, 0.01f, "exact draft");
    ASSERT_NEAR(result.displacement, 4000.0f, 0.1f, "exact displacement");
    ASSERT_NEAR(result.km, 8.0f, 0.01f, "exact KM");
    ASSERT_NEAR(result.kb, 2.0f, 0.01f, "exact KB");
}

static void test_interpolate_midpoint(void) {
    printf("  test_interpolate_midpoint...\n");
    HydroTable table;
    parse_hydro_table(write_test_csv(), &table);

    HydroEntry result;
    hydro_interpolate(&table, 3.0f, &result); /* midpoint between 2.0 and 4.0 */
    ASSERT_NEAR(result.displacement, 3000.0f, 0.1f, "midpoint displacement");
    ASSERT_NEAR(result.km, 9.0f, 0.01f, "midpoint KM");
    ASSERT_NEAR(result.kb, 1.5f, 0.01f, "midpoint KB");
    ASSERT_NEAR(result.bm, 7.5f, 0.01f, "midpoint BM");
}

static void test_interpolate_clamp_low(void) {
    printf("  test_interpolate_clamp_low...\n");
    HydroTable table;
    parse_hydro_table(write_test_csv(), &table);

    HydroEntry result;
    hydro_interpolate(&table, 0.5f, &result); /* below table range */
    ASSERT_NEAR(result.draft, 2.0f, 0.01f, "clamped to first entry");
}

static void test_interpolate_clamp_high(void) {
    printf("  test_interpolate_clamp_high...\n");
    HydroTable table;
    parse_hydro_table(write_test_csv(), &table);

    HydroEntry result;
    hydro_interpolate(&table, 20.0f, &result); /* above table range */
    ASSERT_NEAR(result.draft, 8.0f, 0.01f, "clamped to last entry");
}

static void test_draft_from_displacement(void) {
    printf("  test_draft_from_displacement...\n");
    HydroTable table;
    parse_hydro_table(write_test_csv(), &table);

    /* Exact value */
    float draft = hydro_draft_from_displacement(&table, 4000.0f);
    ASSERT_NEAR(draft, 4.0f, 0.01f, "exact displacement -> draft");

    /* Midpoint */
    draft = hydro_draft_from_displacement(&table, 3000.0f);
    ASSERT_NEAR(draft, 3.0f, 0.01f, "midpoint displacement -> draft");

    /* Quarter point */
    draft = hydro_draft_from_displacement(&table, 5000.0f);
    ASSERT_NEAR(draft, 5.0f, 0.01f, "5000t -> 5.0m draft");
}

int main(void) {
    printf("=== Hydrostatics Tests ===\n");

    test_parse_valid();
    test_parse_reject_unordered();
    test_parse_reject_too_few();
    test_interpolate_exact();
    test_interpolate_midpoint();
    test_interpolate_clamp_low();
    test_interpolate_clamp_high();
    test_draft_from_displacement();

    printf("Hydrostatics: %d/%d tests passed\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
