/*
 * main.c - Entry point for CargoForge-C.
 *
 * This file coordinates the parsing, optimization, and reporting
 * for the maritime cargo logistics simulator.
 */
#include "cargoforge.h"
#include "visualization.h"
#include "json_output.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// The main function controls the program flow.
int main(int argc, char *argv[]) {
    // 1. Check for minimum command-line arguments (ship config + cargo list)
    if (argc < 3) {
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

    // 5. Check output format
    int json_mode = 0;
    int show_viz = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = 1;
            show_viz = 0;  // JSON mode overrides visualization
        } else if (strcmp(argv[i], "--no-viz") == 0) {
            show_viz = 0;
        }
    }

    // 6. Output results
    if (json_mode) {
        // JSON output for API/web interface
        AnalysisResult result = perform_analysis(&ship);
        print_json_output(&ship, &result);
    } else {
        // Human-readable output
        print_loading_plan(&ship);

        if (show_viz) {
            print_cargo_layout_ascii(&ship);
            print_cargo_summary(&ship);
        }
    }

    // 7. Clean up allocated memory.
    free(ship.cargo);
    ship.cargo = NULL;

    return EXIT_SUCCESS;
}

// Prints a usage message to the user.
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <ship_config.cfg> <cargo_list.txt> [options]\n", prog_name);
    fprintf(stderr, "\nArguments:\n");
    fprintf(stderr, "  ship_config.cfg: Ship specifications file\n");
    fprintf(stderr, "  cargo_list.txt:  Cargo manifest file\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --json:          Output results in JSON format (for API/web)\n");
    fprintf(stderr, "  --no-viz:        Disable ASCII visualization output\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s examples/sample_ship.cfg examples/sample_cargo.txt\n", prog_name);
    fprintf(stderr, "  %s ship.cfg cargo.txt --no-viz\n", prog_name);
}