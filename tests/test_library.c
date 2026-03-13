/*
 * test_library.c - Tests for the libcargoforge public API
 */

#include "libcargoforge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

#define ASSERT_EQ_INT(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_GT_FLOAT(a, b, msg) ASSERT((a) > (b), msg)

/* --- Test data --- */

static const char *SHIP_CONFIG =
    "length_m=180\n"
    "width_m=32\n"
    "max_weight_tonnes=50000\n"
    "lightship_weight_tonnes=12000\n"
    "lightship_kg_m=7.5\n";

static const char *CARGO_MANIFEST =
    "CONTAINER1 25000 12.0x2.4x2.6 standard\n"
    "CONTAINER2 18000 6.0x2.4x2.6 standard\n"
    "CONTAINER3 22000 12.0x2.4x2.6 standard\n"
    "DRUMS1 5000 3.0x2.0x1.5 hazardous\n"
    "REEFER1 15000 12.0x2.4x2.9 reefer\n";

/* --- Tests --- */

static void test_version(void) {
    printf("  test_version\n");
    const char *v = cargoforge_version();
    ASSERT(v != NULL, "version not NULL");
    ASSERT(strcmp(v, CF_VERSION_STRING) == 0, "version matches");
}

static void test_error_strings(void) {
    printf("  test_error_strings\n");
    ASSERT(strcmp(cargoforge_errstr(CF_OK), "Success") == 0, "CF_OK string");
    ASSERT(strcmp(cargoforge_errstr(CF_ERR_PARSE), "Parse error") == 0, "CF_ERR_PARSE string");
    ASSERT(strcmp(cargoforge_errstr(-999), "Unknown error") == 0, "unknown code");
}

static void test_open_close(void) {
    printf("  test_open_close\n");
    CargoForge *cf = NULL;
    int rc = cargoforge_open(&cf);
    ASSERT_EQ_INT(rc, CF_OK, "open succeeds");
    ASSERT(cf != NULL, "handle not NULL");

    /* Close NULL is safe */
    cargoforge_close(NULL);

    cargoforge_close(cf);
}

static void test_load_before_ship(void) {
    printf("  test_load_before_ship\n");
    CargoForge *cf;
    cargoforge_open(&cf);

    /* Loading cargo before ship should fail */
    int rc = cargoforge_load_cargo_string(cf, CARGO_MANIFEST);
    ASSERT_EQ_INT(rc, CF_ERR_NO_SHIP, "cargo before ship fails");

    cargoforge_close(cf);
}

static void test_optimize_from_strings(void) {
    printf("  test_optimize_from_strings\n");
    CargoForge *cf;
    cargoforge_open(&cf);

    int rc = cargoforge_load_ship_string(cf, SHIP_CONFIG);
    ASSERT_EQ_INT(rc, CF_OK, "load ship string");

    rc = cargoforge_load_cargo_string(cf, CARGO_MANIFEST);
    ASSERT_EQ_INT(rc, CF_OK, "load cargo string");

    ASSERT_EQ_INT(cargoforge_cargo_count(cf), 5, "cargo count");

    rc = cargoforge_optimize(cf);
    ASSERT_EQ_INT(rc, CF_OK, "optimize succeeds");

    const CfResult *r = cargoforge_result(cf);
    ASSERT(r != NULL, "result not NULL");
    ASSERT_EQ_INT(r->total_count, 5, "total count");
    ASSERT(r->placed_count > 0, "at least 1 placed");
    ASSERT_GT_FLOAT(r->draft, 0.0f, "draft > 0");
    ASSERT_GT_FLOAT(r->gm, 0.0f, "GM > 0");
    ASSERT_GT_FLOAT(r->gm_corrected, 0.0f, "GM corrected > 0");

    /* Check JSON output */
    const char *json = cargoforge_result_json(cf);
    ASSERT(json != NULL, "JSON not NULL");
    ASSERT(strstr(json, "\"placed_count\"") != NULL, "JSON has placed_count");
    ASSERT(strstr(json, "\"gm_corrected\"") != NULL, "JSON has gm_corrected");

    /* Second call returns cached */
    const char *json2 = cargoforge_result_json(cf);
    ASSERT(json == json2, "JSON is cached");

    cargoforge_close(cf);
}

static void test_cargo_info(void) {
    printf("  test_cargo_info\n");
    CargoForge *cf;
    cargoforge_open(&cf);
    cargoforge_load_ship_string(cf, SHIP_CONFIG);
    cargoforge_load_cargo_string(cf, CARGO_MANIFEST);
    cargoforge_optimize(cf);

    CfCargoInfo info;
    int rc = cargoforge_cargo_info(cf, 0, &info);
    ASSERT_EQ_INT(rc, CF_OK, "cargo_info(0) succeeds");
    ASSERT(strlen(info.id) > 0, "first cargo has ID");
    ASSERT(info.weight > 0.0f, "first cargo has weight");

    /* Out of range */
    rc = cargoforge_cargo_info(cf, 100, &info);
    ASSERT_EQ_INT(rc, CF_ERROR, "cargo_info(100) fails");

    cargoforge_close(cf);
}

static void test_imdg_check(void) {
    printf("  test_imdg_check\n");
    CargoForge *cf;
    cargoforge_open(&cf);
    cargoforge_load_ship_string(cf, SHIP_CONFIG);
    cargoforge_load_cargo_string(cf, CARGO_MANIFEST);
    cargoforge_optimize(cf);

    int rc = cargoforge_check_imdg(cf);
    ASSERT_EQ_INT(rc, CF_OK, "imdg check succeeds");

    int compliant = cargoforge_imdg_compliant(cf);
    ASSERT(compliant == 0 || compliant == 1, "imdg compliant is 0 or 1");

    int vcount = cargoforge_imdg_violation_count(cf);
    ASSERT(vcount >= 0, "violation count >= 0");

    cargoforge_close(cf);
}

static void test_reset(void) {
    printf("  test_reset\n");
    CargoForge *cf;
    cargoforge_open(&cf);
    cargoforge_load_ship_string(cf, SHIP_CONFIG);
    cargoforge_load_cargo_string(cf, CARGO_MANIFEST);
    cargoforge_optimize(cf);

    ASSERT(cargoforge_result(cf) != NULL, "result before reset");

    cargoforge_reset(cf);

    ASSERT(cargoforge_result(cf) == NULL, "result after reset");
    ASSERT_EQ_INT(cargoforge_cargo_count(cf), 0, "cargo count after reset");

    /* Can reload after reset */
    int rc = cargoforge_load_ship_string(cf, SHIP_CONFIG);
    ASSERT_EQ_INT(rc, CF_OK, "reload ship after reset");

    cargoforge_close(cf);
}

static void test_optimize_no_data(void) {
    printf("  test_optimize_no_data\n");
    CargoForge *cf;
    cargoforge_open(&cf);

    int rc = cargoforge_optimize(cf);
    ASSERT_EQ_INT(rc, CF_ERR_NO_SHIP, "optimize without ship fails");

    cargoforge_load_ship_string(cf, SHIP_CONFIG);
    rc = cargoforge_optimize(cf);
    ASSERT_EQ_INT(rc, CF_ERR_NO_CARGO, "optimize without cargo fails");

    cargoforge_close(cf);
}

static void test_imdg_before_optimize(void) {
    printf("  test_imdg_before_optimize\n");
    CargoForge *cf;
    cargoforge_open(&cf);
    cargoforge_load_ship_string(cf, SHIP_CONFIG);
    cargoforge_load_cargo_string(cf, CARGO_MANIFEST);

    int rc = cargoforge_check_imdg(cf);
    ASSERT_EQ_INT(rc, CF_ERR_STATE, "imdg before optimize fails");

    cargoforge_close(cf);
}

/* --- Main --- */

int main(void) {
    printf("=== libcargoforge API Tests ===\n\n");

    test_version();
    test_error_strings();
    test_open_close();
    test_load_before_ship();
    test_optimize_from_strings();
    test_cargo_info();
    test_imdg_check();
    test_reset();
    test_optimize_no_data();
    test_imdg_before_optimize();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);

    if (tests_passed != tests_run) {
        fprintf(stderr, "FAILED: %d test(s) failed\n", tests_run - tests_passed);
        return 1;
    }

    printf("All tests passed.\n");
    return 0;
}
