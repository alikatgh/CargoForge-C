/*
 * main.c - Entry point for CargoForge-C.
 *
 * This file coordinates the parsing, optimization, and reporting
 * for the maritime cargo logistics simulator.
 */
#include "cargoforge.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // isatty

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

    // 2. Parse arguments: optional flags (any position) + two file paths.
    bool json = false, strict = false;
    int verbosity = 0;
    enum { COLOR_AUTO, COLOR_ALWAYS, COLOR_NEVER } color_mode = COLOR_AUTO;
    const char *config_file = NULL, *cargo_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict = true;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            verbosity = -1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbosity = 1;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            color_mode = COLOR_NEVER;
        } else if (strncmp(argv[i], "--color=", 8) == 0) {
            const char *v = argv[i] + 8;
            if (strcmp(v, "always") == 0) color_mode = COLOR_ALWAYS;
            else if (strcmp(v, "never") == 0) color_mode = COLOR_NEVER;
            else if (strcmp(v, "auto") == 0) color_mode = COLOR_AUTO;
            else { fprintf(stderr, "Error: --color must be auto, always, or never.\n"); return EXIT_FAILURE; }
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

    // Color is on when forced, or (in auto mode) when stdout is a TTY and the
    // NO_COLOR convention (https://no-color.org) is not set.
    bool use_color = (color_mode == COLOR_ALWAYS) ||
        (color_mode == COLOR_AUTO && isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL);

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
    OutputOptions opt = { use_color, verbosity };
    if (json) print_loading_plan_json(&ship);
    else print_loading_plan(&ship, &opt);

    // 6. In --strict mode, fail the exit code if the plan isn't fully successful:
    //    all cargo placed, within weight, and stable (positive GM). Default mode
    //    always returns success so existing scripts are unaffected.
    int exit_code = EXIT_SUCCESS;
    if (strict) {
        AnalysisResult a = perform_analysis(&ship);
        bool overweight = isnan(a.gm);
        bool all_placed = (a.placed_item_count == ship.cargo_count);
        bool stable = (!overweight && a.gm > 0.15f);
        if (!all_placed || overweight || !stable) {
            fprintf(stderr,
                    "Strict: plan not fully successful (placed %d/%d%s%s).\n",
                    a.placed_item_count, ship.cargo_count,
                    overweight ? ", overweight" : "",
                    (!overweight && !stable) ? ", unstable" : "");
            exit_code = EXIT_FAILURE;
        }
    }

    // 8. Clean up allocated memory.
    free(ship.cargo);
    ship.cargo = NULL;

    return exit_code;
}

// Prints usage/help to stderr.
void usage(const char *prog_name) {
    fprintf(stderr,
        "cargoforge %s - maritime cargo-stowage & stability simulator\n\n"
        "Usage:\n"
        "  %s [options] <ship_config_file> <cargo_list_file>\n"
        "  %s --help | --version\n\n"
        "Options:\n"
        "      --json            Emit the loading plan as JSON (machine-readable)\n"
        "      --strict          Exit non-zero if any cargo is unplaced, overweight, or unstable\n"
        "  -q, --quiet           Print only the load summary\n"
        "      --verbose         Print extra per-item detail\n"
        "      --color=WHEN      Colorize output: auto (default), always, or never\n"
        "      --no-color        Disable colored output\n"
        "  -h, --help            Show this help and exit\n"
        "  -v, --version         Show version and exit\n\n"
        "Example:\n"
        "  %s examples/sample_ship.cfg examples/sample_cargo.txt\n",
        CARGOFORGE_VERSION, prog_name, prog_name, prog_name);
}
