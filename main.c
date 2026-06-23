/*
 * main.c - Entry point for CargoForge-C.
 *
 * This file coordinates the parsing, optimization, and reporting
 * for the maritime cargo logistics simulator.
 */
#include "cargoforge.h"
#include <stdbool.h>
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

    // 2. Parse arguments: an optional --json flag (any position) plus two files.
    bool json = false;
    const char *config_file = NULL, *cargo_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (!config_file) {
            config_file = argv[i];
        } else if (!cargo_file) {
            cargo_file = argv[i];
        } else {
            usage(argv[0]); // too many positional arguments
            return EXIT_FAILURE;
        }
    }
    if (!config_file || !cargo_file) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 3. Initialize the ship struct.
    Ship ship = {0};

    // 4. Parse the input files.
    if (parse_ship_config(config_file, &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse ship config: %s\n", config_file);
        return EXIT_FAILURE;
    }

    if (parse_cargo_list(cargo_file, &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse cargo list: %s\n", cargo_file);
        free(ship.cargo); // Free memory if parsing fails after allocation.
        return EXIT_FAILURE;
    }

    // 5. Run the optimization and analysis.
    optimize_cargo_placement(&ship);
    if (json) print_loading_plan_json(&ship);
    else print_loading_plan(&ship);

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
        "  %s [--json] <ship_config_file> <cargo_list_file>\n"
        "  %s --help | --version\n\n"
        "Options:\n"
        "      --json     Emit the loading plan as JSON (machine-readable)\n"
        "  -h, --help     Show this help and exit\n"
        "  -v, --version  Show version and exit\n\n"
        "Example:\n"
        "  %s examples/sample_ship.cfg examples/sample_cargo.txt\n",
        CARGOFORGE_VERSION, prog_name, prog_name, prog_name);
}
