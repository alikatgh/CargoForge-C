/*
 * main.c - Entry point for CargoForge-C.
 *
 * This file coordinates the parsing, optimization, and reporting
 * for the maritime cargo logistics simulator.
 *
 * To build: make
 * To run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
 */
#include <time.h>    // For clock_gettime
#include "cargoforge.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Start timer for execution measurement
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    Ship ship = {0}; // Initialize struct members to zero/NULL

    // --- Step 1: Parse Input Files ---
    if (parse_ship_config(argv[1], &ship) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_cargo_list(argv[2], &ship) != 0) {
        free(ship.cargo); // Clean up on failure
        return EXIT_FAILURE;
    }
    printf("Successfully parsed ship config and %d cargo items.\n", ship.cargo_count);

    // --- Step 2: Optimize Cargo Placement ---
    optimize_cargo_placement(&ship);

    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double exec_time_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;

    // --- Step 3: Output Results ---
    print_loading_plan(&ship, exec_time_ms);

    // --- Step 4: Cleanup ---
    free(ship.cargo); // Free the dynamically allocated cargo array

    return EXIT_SUCCESS;
}

// Prints the final, formatted loading plan to the console.
void print_loading_plan(const Ship *ship, double exec_time) {
    CG cg = calculate_center_of_gravity(ship);
    int stability_ok = (cg.perc_x >= 45 && cg.perc_x <= 55) && (cg.perc_y >= 45 && cg.perc_y <= 55);
    const char *stability_msg = stability_ok ? "Good" : "Warning";

    printf("\n--- CargoForge 2D Placement Plan ---\n\n");
    printf("Ship Specs: %.2fm x %.2fm | Max Weight: %.2f t\n", ship->length, ship->width, ship->max_weight);
    printf("--------------------------------------------------------------------------\n");
    printf("%-15s | %-20s | %-12s\n", "Cargo ID", "Position (X, Y)", "Weight (t)");
    printf("--------------------------------------------------------------------------\n");

    float total_loaded_weight = 0.0f;
    int placed_count = 0;
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x >= 0) {
            placed_count++;
            total_loaded_weight += c->weight;
            printf("%-15s | (%7.2f m, %7.2f m) | %12.2f\n", c->id, c->pos_x, c->pos_y, c->weight);
        }
    }

    printf("\n--- Load Summary ---\n");
    printf("  Placed/Total Items:  %d / %d\n", placed_count, ship->cargo_count);
    printf("  Total Loaded Weight: %.2f t (%.1f%% of max %.2f t)\n",
           total_loaded_weight, (ship->max_weight > 0) ? (total_loaded_weight / ship->max_weight) * 100.0 : 0.0, ship->max_weight);
    printf("  Center of Gravity:   Lon: %.1f%%, Trans: %.1f%% | Stability: %s\n", cg.perc_x, cg.perc_y, stability_msg);
    printf("  Calculation Time:    %.2f ms\n", exec_time);
    printf("--------------------------------------------------------------------------\n");
}

// Prints usage information to stderr.
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <ship_config_file> <cargo_list_file>\n", prog_name);
    fprintf(stderr, "Example: %s examples/sample_ship.cfg examples/sample_cargo.txt\n", prog_name);
}