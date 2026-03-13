/*
 * test_tanks.c - Tests for tank configuration and free surface correction
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tanks.h"

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

static void test_fsm_partially_filled(void) {
    printf("  test_fsm_partially_filled...\n");
    Tank t = {
        .id = "Test",
        .length = 10.0f,
        .breadth = 6.0f,
        .height = 5.0f,
        .fill_fraction = 0.5f,
        .density = 1.025f
    };

    /* FSM = rho * l * b^3 / 12 = 1.025 * 10 * 216 / 12 = 184.5 */
    float fsm = calculate_free_surface_moment(&t);
    ASSERT_NEAR(fsm, 184.5f, 0.1f, "FSM for partially filled tank");
}

static void test_fsm_full_tank(void) {
    printf("  test_fsm_full_tank...\n");
    Tank t = {
        .id = "Full",
        .length = 10.0f,
        .breadth = 6.0f,
        .height = 5.0f,
        .fill_fraction = 1.0f,
        .density = 1.025f
    };

    float fsm = calculate_free_surface_moment(&t);
    ASSERT_NEAR(fsm, 0.0f, 0.001f, "FSM for full tank should be 0");
}

static void test_fsm_empty_tank(void) {
    printf("  test_fsm_empty_tank...\n");
    Tank t = {
        .id = "Empty",
        .length = 10.0f,
        .breadth = 6.0f,
        .height = 5.0f,
        .fill_fraction = 0.0f,
        .density = 1.025f
    };

    float fsm = calculate_free_surface_moment(&t);
    ASSERT_NEAR(fsm, 0.0f, 0.001f, "FSM for empty tank should be 0");
}

static void test_total_fsm(void) {
    printf("  test_total_fsm...\n");
    TankConfig config;
    memset(&config, 0, sizeof(config));
    config.count = 2;

    /* Tank 1: partially filled */
    config.tanks[0] = (Tank){
        .id = "T1", .length = 10.0f, .breadth = 6.0f, .height = 5.0f,
        .fill_fraction = 0.5f, .density = 1.025f
    };
    /* Tank 2: partially filled */
    config.tanks[1] = (Tank){
        .id = "T2", .length = 8.0f, .breadth = 4.0f, .height = 4.0f,
        .fill_fraction = 0.3f, .density = 0.95f
    };

    /* T1 FSM = 1.025 * 10 * 216 / 12 = 184.5 */
    /* T2 FSM = 0.95 * 8 * 64 / 12 = 40.533 */
    float total = calculate_total_fsm(&config);
    ASSERT_NEAR(total, 225.03f, 0.1f, "total FSM of two tanks");
}

static void test_virtual_kg_rise(void) {
    printf("  test_virtual_kg_rise...\n");
    TankConfig config;
    memset(&config, 0, sizeof(config));
    config.count = 1;
    config.tanks[0] = (Tank){
        .id = "T1", .length = 10.0f, .breadth = 6.0f, .height = 5.0f,
        .fill_fraction = 0.5f, .density = 1.025f
    };

    /* GG' = FSM / displacement = 184.5 / 5000 = 0.0369 */
    float gg = calculate_virtual_kg_rise(&config, 5000.0f);
    ASSERT_NEAR(gg, 0.0369f, 0.001f, "virtual KG rise");
}

static void test_tank_weight(void) {
    printf("  test_tank_weight...\n");
    TankConfig config;
    memset(&config, 0, sizeof(config));
    config.count = 1;
    config.tanks[0] = (Tank){
        .id = "T1", .length = 10.0f, .breadth = 6.0f, .height = 5.0f,
        .fill_fraction = 0.5f, .density = 1.025f
    };

    /* Weight = 10 * 6 * 5 * 0.5 * 1.025 = 153.75 t */
    float weight = calculate_tank_weight(&config);
    ASSERT_NEAR(weight, 153.75f, 0.01f, "tank weight");
}

static void test_parse_tank_csv(void) {
    printf("  test_parse_tank_csv...\n");
    const char *path = "/tmp/test_tanks.csv";
    FILE *fp = fopen(path, "w");
    fprintf(fp, "# Test tank config\n");
    fprintf(fp, "BallastFP,8.0,12.0,6.0,140.0,0.0,0.0,0.50,1.025\n");
    fprintf(fp, "FuelOil,10.0,6.0,5.0,30.0,-8.0,0.0,0.60,0.950\n");
    fclose(fp);

    TankConfig config;
    int ret = parse_tank_config(path, &config);
    ASSERT(ret == 0, "parse returns 0");
    ASSERT(config.count == 2, "2 tanks parsed");
    ASSERT(strcmp(config.tanks[0].id, "BallastFP") == 0, "first tank id");
    ASSERT_NEAR(config.tanks[0].fill_fraction, 0.5f, 0.01f, "fill fraction");
    ASSERT_NEAR(config.tanks[1].density, 0.95f, 0.01f, "fuel oil density");
}

int main(void) {
    printf("=== Tanks Tests ===\n");

    test_fsm_partially_filled();
    test_fsm_full_tank();
    test_fsm_empty_tank();
    test_total_fsm();
    test_virtual_kg_rise();
    test_tank_weight();
    test_parse_tank_csv();

    printf("Tanks: %d/%d tests passed\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
