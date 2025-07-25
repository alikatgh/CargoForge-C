/*
 * main.c - Entry point for CargoForge-C.
 *
 * This file coordinates the parsing, optimization, and reporting
 * for the maritime cargo logistics simulator.
 */
#include "cargoforge.h"
#include <stdio.h>
#include <stdlib.h>

// The main function controls the program flow.
int main(int argc, char *argv[]) {
    // 1. Check for correct command-line arguments.
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 2. Initialize the ship struct.
    Ship ship = {0};

    // 3. Parse the input files.
    if (parse_ship_config(argv[1], &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse ship config: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (parse_cargo_list(argv[2], &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse cargo list: %s\n", argv[2]);
        free(ship.cargo); // Free memory if parsing fails after allocation.
        return EXIT_FAILURE;
    }

    // 4. Run the optimization and analysis.
    optimize_cargo_placement(&ship);
    print_loading_plan(&ship);

    // 5. Clean up allocated memory.
    free(ship.cargo);
    ship.cargo = NULL;

    return EXIT_SUCCESS;
}

// Prints a usage message to the user.
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <ship_config_file> <cargo_list_file>\n", prog_name);
    fprintf(stderr, "Example: %s examples/sample_ship.cfg examples/sample_cargo.txt\n", prog_name);
}