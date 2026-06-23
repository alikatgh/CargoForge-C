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
    // 1. Handle informational flags before requiring file arguments.
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS; // --help is a successful, requested action.
        }
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("cargoforge %s\n", CARGOFORGE_VERSION);
            return EXIT_SUCCESS;
        }
    }

    // 2. Otherwise we need exactly two file arguments.
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 3. Initialize the ship struct.
    Ship ship = {0};

    // 4. Parse the input files.
    if (parse_ship_config(argv[1], &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse ship config: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (parse_cargo_list(argv[2], &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse cargo list: %s\n", argv[2]);
        free(ship.cargo); // Free memory if parsing fails after allocation.
        return EXIT_FAILURE;
    }

    // 5. Run the optimization and analysis.
    optimize_cargo_placement(&ship);
    print_loading_plan(&ship);

    // 6. Clean up allocated memory.
    free(ship.cargo);
    ship.cargo = NULL;

    return EXIT_SUCCESS;
}

// Prints usage/help to stderr.
void usage(const char *prog_name) {
    fprintf(stderr,
        "cargoforge %s - maritime cargo-stowage & stability simulator\n\n"
        "Usage:\n"
        "  %s <ship_config_file> <cargo_list_file>\n"
        "  %s --help | --version\n\n"
        "Options:\n"
        "  -h, --help     Show this help and exit\n"
        "  -v, --version  Show version and exit\n\n"
        "Example:\n"
        "  %s examples/sample_ship.cfg examples/sample_cargo.txt\n",
        CARGOFORGE_VERSION, prog_name, prog_name, prog_name);
}