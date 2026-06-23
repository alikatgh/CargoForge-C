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

// Prints a ready-to-use, commented sample ship config to stdout (`--init`).
static void print_config_template(void) {
    printf(
        "# CargoForge ship configuration (edit and save, e.g. `cargoforge --init > ship.cfg`)\n"
        "length_m=150\n"
        "width_m=25\n"
        "max_weight_tonnes=20000\n"
        "lightship_weight_tonnes=5000   # empty-ship weight\n"
        "lightship_kg_m=8               # empty-ship vertical CG above keel\n"
        "holds=2                        # below-deck holds (optional, default 2)\n"
        "depth_m=14                     # moulded hull depth (optional, enables freeboard)\n");
}

// Echoes the parsed, normalized ship config (`--show-config`), for verification.
static void print_parsed_config(const Ship *ship) {
    printf("length_m=%.2f\n", ship->length);
    printf("width_m=%.2f\n", ship->width);
    printf("max_weight_tonnes=%.2f\n", ship->max_weight / 1000.0f);
    printf("lightship_weight_tonnes=%.2f\n", ship->lightship_weight / 1000.0f);
    printf("lightship_kg_m=%.2f\n", ship->lightship_kg);
    printf("holds=%d\n", ship->hold_count > 0 ? ship->hold_count : DEFAULT_HOLDS);
    if (ship->depth > 0.0f) printf("depth_m=%.2f\n", ship->depth);
}

// Applies CARGOFORGE_* environment overrides on top of the parsed config.
static void apply_env_overrides(Ship *ship) {
    const char *e;
    if ((e = getenv("CARGOFORGE_HOLDS")) != NULL) {
        int h = atoi(e);
        if (h >= 1 && h <= MAX_HOLDS) ship->hold_count = h;
        else fprintf(stderr, "Warning: CARGOFORGE_HOLDS=%s out of range, ignored.\n", e);
    }
    if ((e = getenv("CARGOFORGE_DEPTH_M")) != NULL) {
        double d = atof(e);
        if (d > 0.0) ship->depth = (float)d;
    }
}

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
        if (strcmp(argv[1], "--init") == 0) {
            print_config_template(); // sample ship config to stdout; redirect to a file
            return EXIT_SUCCESS;
        }
    }

    // 2. Parse arguments: optional flags (any position) + two file paths.
    bool json = false, csv = false, md = false, strict = false, diagram = false;
    bool show_config = false;
    int verbosity = 0;
    enum { COLOR_AUTO, COLOR_ALWAYS, COLOR_NEVER } color_mode = COLOR_AUTO;
    const char *config_file = NULL, *cargo_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-config") == 0) {
            show_config = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (strcmp(argv[i], "--csv") == 0) {
            csv = true;
        } else if (strcmp(argv[i], "--md") == 0) {
            md = true;
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict = true;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            verbosity = -1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbosity = 1;
        } else if (strcmp(argv[i], "--diagram") == 0) {
            diagram = true;
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
    if (!config_file) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // --show-config only needs the ship config: parse it, echo it back, and exit.
    if (show_config) {
        Ship cfg = {0};
        if (parse_ship_config(config_file, &cfg) != 0) {
            fprintf(stderr, "Error: Failed to parse ship config: %s\n", config_file);
            return EXIT_FAILURE;
        }
        apply_env_overrides(&cfg);
        print_parsed_config(&cfg);
        return EXIT_SUCCESS;
    }

    if (!cargo_file) {
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
    apply_env_overrides(&ship); // CARGOFORGE_* env vars override the config file

    if (parse_cargo_list(cargo_file, &ship) != 0) {
        fprintf(stderr, "Error: Failed to parse cargo list: %s\n", cargo_file);
        free(ship.cargo); // Free memory if parsing fails after allocation.
        return EXIT_FAILURE;
    }

    // 5. Run the optimization and analysis.
    optimize_cargo_placement(&ship);
    OutputOptions opt = { use_color, verbosity, diagram };
    if (json) print_loading_plan_json(&ship);
    else if (csv) print_loading_plan_csv(&ship);
    else if (md) print_loading_plan_md(&ship);
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
        "  %s --init | --help | --version\n\n"
        "  Use '-' for a file path to read it from stdin.\n"
        "  Env: CARGOFORGE_HOLDS, CARGOFORGE_DEPTH_M override the config; NO_COLOR disables color.\n\n"
        "Options:\n"
        "      --init            Print a sample ship config to stdout, then exit\n"
        "      --show-config     Parse the ship config, print it normalized, then exit\n"
        "      --json            Emit the loading plan as JSON (machine-readable)\n"
        "      --csv             Emit the placements as CSV\n"
        "      --md              Emit a Markdown report\n"
        "      --strict          Exit non-zero if any cargo is unplaced, overweight, or unstable\n"
        "  -q, --quiet           Print only the load summary\n"
        "      --verbose         Print extra per-item detail\n"
        "      --diagram         Draw an ASCII stowage plan, utilization bars, and GM gauge\n"
        "      --color=WHEN      Colorize output: auto (default), always, or never\n"
        "      --no-color        Disable colored output\n"
        "  -h, --help            Show this help and exit\n"
        "  -v, --version         Show version and exit\n\n"
        "Example:\n"
        "  %s examples/sample_ship.cfg examples/sample_cargo.txt\n",
        CARGOFORGE_VERSION, prog_name, prog_name, prog_name);
}
