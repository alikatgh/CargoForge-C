/*
 * cli.c - Command-line interface implementation
 *
 * This file implements the modern CLI for CargoForge-C with subcommands,
 * advanced options, and multiple output formats.
 */

#include "cli.h"
#include "visualization.h"
#include "json_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>

// Global context for color support (simplified access)
static CLIContext *g_ctx = NULL;

// Initialize CLI context with default values
void init_cli_context(CLIContext *ctx) {
    ctx->subcommand = NULL;
    ctx->ship_file = NULL;
    ctx->cargo_file = NULL;
    ctx->results_file = NULL;
    ctx->format = FORMAT_HUMAN;
    ctx->output_file = NULL;
    ctx->verbose = false;
    ctx->quiet = false;
    ctx->show_viz = true;
    ctx->color = isatty(STDERR_FILENO); // Auto-detect color support
    ctx->algorithm = NULL;
    ctx->max_iterations = 1000;
    ctx->timeout_seconds = 60;
    ctx->only_placed = false;
    ctx->only_failed = false;
    ctx->cargo_type_filter = NULL;
    ctx->interactive = false;
    g_ctx = ctx;
}

// Free allocated memory in CLI context
void free_cli_context(CLIContext *ctx) {
    // Note: We don't free string literals, only dynamically allocated strings
    // The argv strings are managed by the system, so we don't free those either
    g_ctx = NULL;
}

// Check if filename represents stdin
bool is_stdin(const char *filename) {
    return filename && strcmp(filename, "-") == 0;
}

// Print version information
void print_version(void) {
    printf("CargoForge-C version %s\n", CARGOFORGE_VERSION);
    printf("Build date: %s\n", CARGOFORGE_BUILD_DATE);
    printf("Maritime cargo optimization engine\n");
    printf("\nCopyright (c) 2025 CargoForge Project\n");
    printf("Licensed under MIT License\n");
}

// Print general help
void print_general_help(const char *prog_name) {
    printf("CargoForge-C - Maritime Cargo Optimization Engine\n\n");
    printf("USAGE:\n");
    printf("  %s <subcommand> [options]\n\n", prog_name);

    printf("SUBCOMMANDS:\n");
    printf("  optimize    Run cargo optimization and placement algorithm\n");
    printf("  validate    Validate ship configuration and cargo manifest\n");
    printf("  info        Display ship and cargo statistics\n");
    printf("  analyze     Analyze optimization results from JSON file\n");
    printf("  interactive Start interactive configuration wizard\n");
    printf("  version     Display version information\n");
    printf("  help        Show this help message\n\n");

    printf("GLOBAL OPTIONS:\n");
    printf("  -h, --help           Show help message\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -q, --quiet          Suppress non-essential output\n");
    printf("  --no-color           Disable colored output\n");
    printf("  --version            Display version information\n\n");

    printf("EXAMPLES:\n");
    printf("  # Run optimization\n");
    printf("  %s optimize ship.cfg cargo.txt\n\n", prog_name);
    printf("  # Validate inputs before optimization\n");
    printf("  %s validate ship.cfg cargo.txt\n\n", prog_name);
    printf("  # Get ship information\n");
    printf("  %s info ship.cfg\n\n", prog_name);
    printf("  # JSON output for API integration\n");
    printf("  %s optimize ship.cfg cargo.txt --format=json\n\n", prog_name);
    printf("  # Interactive mode\n");
    printf("  %s interactive\n\n", prog_name);

    printf("For subcommand-specific help, use:\n");
    printf("  %s <subcommand> --help\n\n", prog_name);
}

// Print subcommand-specific help
void print_subcommand_help(const char *subcommand) {
    if (strcmp(subcommand, "optimize") == 0) {
        printf("CargoForge-C - optimize subcommand\n\n");
        printf("USAGE:\n");
        printf("  cargoforge optimize <ship_config> <cargo_manifest> [options]\n\n");
        printf("DESCRIPTION:\n");
        printf("  Runs the 3D bin-packing optimization algorithm to find optimal\n");
        printf("  cargo placement on the ship while maintaining stability and\n");
        printf("  safety constraints.\n\n");
        printf("ARGUMENTS:\n");
        printf("  ship_config      Ship configuration file (.cfg format)\n");
        printf("  cargo_manifest   Cargo manifest file (.txt format)\n\n");
        printf("OPTIONS:\n");
        printf("  --format=FORMAT      Output format: human|json|csv|table|markdown\n");
        printf("  --output=FILE        Write output to file instead of stdout\n");
        printf("  --algorithm=ALGO     Algorithm: 3d|2d|auto (default: 3d)\n");
        printf("  --no-viz             Disable ASCII visualization\n");
        printf("  --only-placed        Show only successfully placed cargo\n");
        printf("  --only-failed        Show only failed cargo items\n");
        printf("  --type=TYPE          Filter by cargo type\n");
        printf("  -v, --verbose        Enable verbose output\n");
        printf("  -h, --help           Show this help message\n\n");
        printf("EXAMPLES:\n");
        printf("  cargoforge optimize ship.cfg cargo.txt\n");
        printf("  cargoforge optimize ship.cfg cargo.txt --format=json\n");
        printf("  cargoforge optimize ship.cfg cargo.txt --only-failed\n");
        printf("  cargoforge optimize ship.cfg cargo.txt --output=results.json --format=json\n");
    }
    else if (strcmp(subcommand, "validate") == 0) {
        printf("CargoForge-C - validate subcommand\n\n");
        printf("USAGE:\n");
        printf("  cargoforge validate <ship_config> <cargo_manifest> [options]\n\n");
        printf("DESCRIPTION:\n");
        printf("  Validates ship configuration and cargo manifest files without\n");
        printf("  running optimization. Checks for syntax errors, invalid values,\n");
        printf("  and constraint violations.\n\n");
        printf("ARGUMENTS:\n");
        printf("  ship_config      Ship configuration file\n");
        printf("  cargo_manifest   Cargo manifest file\n\n");
        printf("OPTIONS:\n");
        printf("  --format=FORMAT      Output format: human|json|table\n");
        printf("  -v, --verbose        Show detailed validation info\n");
        printf("  -h, --help           Show this help message\n\n");
        printf("EXAMPLES:\n");
        printf("  cargoforge validate ship.cfg cargo.txt\n");
        printf("  cargoforge validate ship.cfg cargo.txt --verbose\n");
    }
    else if (strcmp(subcommand, "info") == 0) {
        printf("CargoForge-C - info subcommand\n\n");
        printf("USAGE:\n");
        printf("  cargoforge info <ship_config> [cargo_manifest] [options]\n\n");
        printf("DESCRIPTION:\n");
        printf("  Displays detailed information about ship specifications and\n");
        printf("  cargo statistics without running optimization.\n\n");
        printf("ARGUMENTS:\n");
        printf("  ship_config      Ship configuration file\n");
        printf("  cargo_manifest   Optional cargo manifest file\n\n");
        printf("OPTIONS:\n");
        printf("  --format=FORMAT      Output format: human|json|table|markdown\n");
        printf("  -h, --help           Show this help message\n\n");
        printf("EXAMPLES:\n");
        printf("  cargoforge info ship.cfg\n");
        printf("  cargoforge info ship.cfg cargo.txt\n");
        printf("  cargoforge info ship.cfg --format=json\n");
    }
    else if (strcmp(subcommand, "interactive") == 0) {
        printf("CargoForge-C - interactive subcommand\n\n");
        printf("USAGE:\n");
        printf("  cargoforge interactive\n\n");
        printf("DESCRIPTION:\n");
        printf("  Starts an interactive wizard that guides you through creating\n");
        printf("  ship configuration and cargo manifest files step-by-step.\n\n");
        printf("EXAMPLES:\n");
        printf("  cargoforge interactive\n");
    }
    else {
        printf("No help available for subcommand: %s\n", subcommand);
        printf("Use 'cargoforge help' for general help.\n");
    }
}

// Print examples
void print_examples(void) {
    printf("CargoForge-C - Usage Examples\n\n");

    printf("BASIC OPTIMIZATION:\n");
    printf("  cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt\n\n");

    printf("JSON OUTPUT FOR API:\n");
    printf("  cargoforge optimize ship.cfg cargo.txt --format=json > results.json\n\n");

    printf("VALIDATE BEFORE RUNNING:\n");
    printf("  cargoforge validate ship.cfg cargo.txt && \\\n");
    printf("  cargoforge optimize ship.cfg cargo.txt\n\n");

    printf("GET SHIP INFO:\n");
    printf("  cargoforge info examples/sample_ship.cfg\n\n");

    printf("FILTER RESULTS:\n");
    printf("  cargoforge optimize ship.cfg cargo.txt --only-failed\n");
    printf("  cargoforge optimize ship.cfg cargo.txt --type=hazardous\n\n");

    printf("CSV EXPORT:\n");
    printf("  cargoforge optimize ship.cfg cargo.txt --format=csv > cargo_plan.csv\n\n");

    printf("MARKDOWN REPORT:\n");
    printf("  cargoforge optimize ship.cfg cargo.txt --format=markdown > report.md\n\n");
}

// Utility: Print colored messages
void print_success(const char *message) {
    if (g_ctx && g_ctx->color) {
        fprintf(stderr, COLOR_GREEN "✓ " COLOR_RESET "%s\n", message);
    } else {
        fprintf(stderr, "[OK] %s\n", message);
    }
}

void print_error_with_context(const char *filename, int line, const char *message) {
    if (g_ctx && g_ctx->color) {
        fprintf(stderr, COLOR_RED "✗ Error" COLOR_RESET " in %s", filename);
        if (line > 0) {
            fprintf(stderr, " (line %d)", line);
        }
        fprintf(stderr, ": %s\n", message);
    } else {
        fprintf(stderr, "[ERROR] %s", filename);
        if (line > 0) {
            fprintf(stderr, " (line %d)", line);
        }
        fprintf(stderr, ": %s\n", message);
    }
}

void print_warning(const char *message) {
    if (g_ctx && g_ctx->color) {
        fprintf(stderr, COLOR_YELLOW "⚠ Warning:" COLOR_RESET " %s\n", message);
    } else {
        fprintf(stderr, "[WARNING] %s\n", message);
    }
}

// Get cargo type symbol for display
const char *get_cargo_type_symbol(const char *type) {
    if (strcmp(type, "hazardous") == 0) return "⚠";
    if (strcmp(type, "reefer") == 0) return "❄";
    if (strcmp(type, "fragile") == 0) return "⚡";
    return "■";
}

// Get stability status color
const char *get_stability_color(float gm) {
    if (gm < 0.5) return COLOR_RED;
    if (gm > 2.5) return COLOR_YELLOW;
    return COLOR_GREEN;
}

// Parse command-line arguments
int parse_cli_args(int argc, char *argv[], CLIContext *ctx) {
    if (argc < 2) {
        print_general_help(argv[0]);
        return -1;
    }

    // First argument should be subcommand (unless it's --help or --version)
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_general_help(argv[0]);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        print_version();
        return 0;
    }

    // Extract subcommand
    ctx->subcommand = argv[1];

    // Define long options for getopt_long
    static struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"verbose",     no_argument,       0, 'v'},
        {"quiet",       no_argument,       0, 'q'},
        {"format",      required_argument, 0, 'f'},
        {"output",      required_argument, 0, 'o'},
        {"algorithm",   required_argument, 0, 'a'},
        {"no-viz",      no_argument,       0, 'n'},
        {"no-color",    no_argument,       0, 'c'},
        {"only-placed", no_argument,       0, 'p'},
        {"only-failed", no_argument,       0, 'F'},
        {"type",        required_argument, 0, 't'},
        {"json",        no_argument,       0, 'j'}, // Backward compatibility
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    // Reset getopt for parsing from argv[2] onwards
    optind = 2;

    while ((opt = getopt_long(argc, argv, "hvqf:o:a:t:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_subcommand_help(ctx->subcommand);
                return 0;
            case 'v':
                ctx->verbose = true;
                break;
            case 'q':
                ctx->quiet = true;
                break;
            case 'f':
                if (strcmp(optarg, "json") == 0) ctx->format = FORMAT_JSON;
                else if (strcmp(optarg, "csv") == 0) ctx->format = FORMAT_CSV;
                else if (strcmp(optarg, "table") == 0) ctx->format = FORMAT_TABLE;
                else if (strcmp(optarg, "markdown") == 0) ctx->format = FORMAT_MARKDOWN;
                else if (strcmp(optarg, "human") == 0) ctx->format = FORMAT_HUMAN;
                else {
                    fprintf(stderr, "Error: Unknown format '%s'\n", optarg);
                    return -1;
                }
                break;
            case 'o':
                ctx->output_file = optarg;
                break;
            case 'a':
                ctx->algorithm = optarg;
                break;
            case 'n':
                ctx->show_viz = false;
                break;
            case 'c':
                ctx->color = false;
                break;
            case 'p':
                ctx->only_placed = true;
                break;
            case 'F':
                ctx->only_failed = true;
                break;
            case 't':
                ctx->cargo_type_filter = optarg;
                break;
            case 'j':
                // Backward compatibility for --json
                ctx->format = FORMAT_JSON;
                ctx->show_viz = false;
                break;
            default:
                return -1;
        }
    }

    // Parse positional arguments (files)
    if (optind < argc) {
        ctx->ship_file = argv[optind++];
    }
    if (optind < argc) {
        ctx->cargo_file = argv[optind++];
    }
    if (optind < argc) {
        ctx->results_file = argv[optind++];
    }

    return 1; // Success, continue execution
}

// Dispatch to appropriate subcommand handler
int dispatch_subcommand(CLIContext *ctx) {
    if (!ctx->subcommand) {
        fprintf(stderr, "Error: No subcommand specified\n");
        return EXIT_INVALID_ARGS;
    }

    // Match subcommand
    if (strcmp(ctx->subcommand, "optimize") == 0) {
        return cmd_optimize(ctx);
    }
    else if (strcmp(ctx->subcommand, "validate") == 0) {
        return cmd_validate(ctx);
    }
    else if (strcmp(ctx->subcommand, "info") == 0) {
        return cmd_info(ctx);
    }
    else if (strcmp(ctx->subcommand, "analyze") == 0) {
        return cmd_analyze(ctx);
    }
    else if (strcmp(ctx->subcommand, "interactive") == 0) {
        return cmd_interactive(ctx);
    }
    else if (strcmp(ctx->subcommand, "version") == 0) {
        return cmd_version(ctx);
    }
    else if (strcmp(ctx->subcommand, "help") == 0) {
        return cmd_help(ctx);
    }
    else {
        fprintf(stderr, "Error: Unknown subcommand '%s'\n", ctx->subcommand);
        fprintf(stderr, "Use 'cargoforge help' for available commands\n");
        return EXIT_INVALID_ARGS;
    }
}

// SUBCOMMAND: optimize
int cmd_optimize(CLIContext *ctx) {
    if (!ctx->ship_file || !ctx->cargo_file) {
        fprintf(stderr, "Error: 'optimize' requires ship config and cargo manifest\n");
        fprintf(stderr, "Usage: cargoforge optimize <ship_config> <cargo_manifest> [options]\n");
        return EXIT_INVALID_ARGS;
    }

    if (!ctx->quiet) {
        fprintf(stderr, "CargoForge-C Optimizer v%s\n", CARGOFORGE_VERSION);
        fprintf(stderr, "Ship: %s\n", ctx->ship_file);
        fprintf(stderr, "Cargo: %s\n", ctx->cargo_file);
        fprintf(stderr, "\n");
    }

    // Initialize ship
    Ship ship = {0};

    // Parse ship config
    if (ctx->verbose) fprintf(stderr, "Parsing ship configuration...\n");
    if (parse_ship_config(ctx->ship_file, &ship) != 0) {
        print_error_with_context(ctx->ship_file, 0, "Failed to parse ship configuration");
        return EXIT_PARSE_ERROR;
    }
    if (!ctx->quiet) print_success("Ship configuration loaded");

    // Parse cargo manifest
    if (ctx->verbose) fprintf(stderr, "Parsing cargo manifest...\n");
    if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
        print_error_with_context(ctx->cargo_file, 0, "Failed to parse cargo manifest");
        free(ship.cargo);
        return EXIT_PARSE_ERROR;
    }
    if (!ctx->quiet) print_success("Cargo manifest loaded");

    // Run optimization
    if (!ctx->quiet) {
        fprintf(stderr, "\nRunning optimization algorithm...\n");
    }
    optimize_cargo_placement(&ship);
    if (!ctx->quiet) print_success("Optimization complete");

    // Perform analysis
    AnalysisResult result = perform_analysis(&ship);

    // Output results
    output_results(&ship, &result, ctx->format, ctx->output_file);

    // Cleanup
    free(ship.cargo);
    return EXIT_SUCCESS;
}

// SUBCOMMAND: validate
int cmd_validate(CLIContext *ctx) {
    if (!ctx->ship_file || !ctx->cargo_file) {
        fprintf(stderr, "Error: 'validate' requires ship config and cargo manifest\n");
        fprintf(stderr, "Usage: cargoforge validate <ship_config> <cargo_manifest>\n");
        return EXIT_INVALID_ARGS;
    }

    if (!ctx->quiet) {
        fprintf(stderr, "CargoForge-C Validator\n");
        fprintf(stderr, "======================\n\n");
    }

    Ship ship = {0};
    int errors = 0;

    // Validate ship config
    fprintf(stderr, "Validating ship configuration: %s\n", ctx->ship_file);
    if (parse_ship_config(ctx->ship_file, &ship) != 0) {
        print_error_with_context(ctx->ship_file, 0, "Invalid ship configuration");
        errors++;
    } else {
        print_success("Ship configuration is valid");

        if (ctx->verbose) {
            printf("  Length: %.2f m\n", ship.length);
            printf("  Width: %.2f m\n", ship.width);
            printf("  Max weight: %.0f kg\n", ship.max_weight);
        }
    }

    // Validate cargo manifest
    fprintf(stderr, "\nValidating cargo manifest: %s\n", ctx->cargo_file);
    if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
        print_error_with_context(ctx->cargo_file, 0, "Invalid cargo manifest");
        errors++;
    } else {
        print_success("Cargo manifest is valid");

        if (ctx->verbose) {
            printf("  Total items: %d\n", ship.cargo_count);

            float total_weight = 0;
            for (int i = 0; i < ship.cargo_count; i++) {
                total_weight += ship.cargo[i].weight;
            }
            printf("  Total weight: %.0f kg\n", total_weight);

            if (total_weight > ship.max_weight) {
                print_warning("Total cargo weight exceeds ship capacity!");
            }
        }
    }

    // Summary
    fprintf(stderr, "\n");
    if (errors == 0) {
        print_success("All validation checks passed!");
        if (ship.cargo) free(ship.cargo);
        return EXIT_SUCCESS;
    } else {
        if (g_ctx && g_ctx->color) {
            fprintf(stderr, COLOR_RED "✗ Validation failed with %d error(s)\n" COLOR_RESET, errors);
        } else {
            fprintf(stderr, "[FAILED] Validation failed with %d error(s)\n", errors);
        }
        if (ship.cargo) free(ship.cargo);
        return EXIT_VALIDATION_ERROR;
    }
}

// SUBCOMMAND: info
int cmd_info(CLIContext *ctx) {
    if (!ctx->ship_file) {
        fprintf(stderr, "Error: 'info' requires ship config file\n");
        fprintf(stderr, "Usage: cargoforge info <ship_config> [cargo_manifest]\n");
        return EXIT_INVALID_ARGS;
    }

    Ship ship = {0};

    // Parse ship config
    if (parse_ship_config(ctx->ship_file, &ship) != 0) {
        print_error_with_context(ctx->ship_file, 0, "Failed to parse ship configuration");
        return EXIT_PARSE_ERROR;
    }

    // Optionally parse cargo
    if (ctx->cargo_file) {
        if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
            print_error_with_context(ctx->cargo_file, 0, "Failed to parse cargo manifest");
            free(ship.cargo);
            return EXIT_PARSE_ERROR;
        }
    }

    // Display info
    output_ship_info(&ship, ctx->format);

    // Cleanup
    if (ship.cargo) free(ship.cargo);
    return EXIT_SUCCESS;
}

// SUBCOMMAND: analyze (placeholder for future enhancement)
int cmd_analyze(CLIContext *ctx) {
    fprintf(stderr, "Analyze subcommand is not yet implemented.\n");
    fprintf(stderr, "This will analyze saved JSON results in a future version.\n");
    return EXIT_SUCCESS;
}

// SUBCOMMAND: interactive
int cmd_interactive(CLIContext *ctx) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║      CargoForge-C Interactive Configuration Wizard      ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    printf("This wizard will guide you through creating ship and cargo files.\n\n");

    char ship_file[256] = {0};
    char cargo_file[256] = {0};

    // Ship configuration
    printf("STEP 1: Ship Configuration\n");
    printf("---------------------------\n");

    float length, width, max_weight;
    printf("Enter ship length (meters): ");
    if (scanf("%f", &length) != 1 || length <= 0) {
        fprintf(stderr, "Invalid length\n");
        return EXIT_INVALID_ARGS;
    }

    printf("Enter ship width (meters): ");
    if (scanf("%f", &width) != 1 || width <= 0) {
        fprintf(stderr, "Invalid width\n");
        return EXIT_INVALID_ARGS;
    }

    printf("Enter maximum cargo weight (tonnes): ");
    if (scanf("%f", &max_weight) != 1 || max_weight <= 0) {
        fprintf(stderr, "Invalid weight\n");
        return EXIT_INVALID_ARGS;
    }

    printf("Enter filename to save ship config: ");
    if (scanf("%255s", ship_file) != 1) {
        fprintf(stderr, "Invalid filename\n");
        return EXIT_INVALID_ARGS;
    }

    // Write ship config file
    FILE *fp = fopen(ship_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create file %s\n", ship_file);
        return EXIT_FILE_ERROR;
    }

    fprintf(fp, "# Ship Configuration - Generated by CargoForge-C Interactive Mode\n");
    fprintf(fp, "length_m=%.2f\n", length);
    fprintf(fp, "width_m=%.2f\n", width);
    fprintf(fp, "max_weight_tonnes=%.2f\n", max_weight);
    fprintf(fp, "lightship_weight_tonnes=%.2f\n", max_weight * 0.1); // Default 10%
    fprintf(fp, "lightship_kg_m=%.2f\n", length / 2.0); // Default mid-ship
    fclose(fp);

    print_success("Ship configuration saved");

    // Cargo configuration
    printf("\nSTEP 2: Cargo Configuration\n");
    printf("----------------------------\n");

    int num_items;
    printf("How many cargo items? ");
    if (scanf("%d", &num_items) != 1 || num_items <= 0) {
        fprintf(stderr, "Invalid number\n");
        return EXIT_INVALID_ARGS;
    }

    printf("Enter filename to save cargo manifest: ");
    if (scanf("%255s", cargo_file) != 1) {
        fprintf(stderr, "Invalid filename\n");
        return EXIT_INVALID_ARGS;
    }

    fp = fopen(cargo_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create file %s\n", cargo_file);
        return EXIT_FILE_ERROR;
    }

    fprintf(fp, "# Cargo Manifest - Generated by CargoForge-C Interactive Mode\n");
    fprintf(fp, "# Format: ID Weight(tonnes) Length Width Height Type\n\n");

    for (int i = 0; i < num_items; i++) {
        char id[32];
        float weight, l, w, h;
        char type[16];

        printf("\nCargo item %d:\n", i + 1);
        printf("  ID: ");
        if (scanf("%31s", id) != 1) continue;

        printf("  Weight (tonnes): ");
        if (scanf("%f", &weight) != 1) continue;

        printf("  Dimensions (L W H in meters): ");
        if (scanf("%f %f %f", &l, &w, &h) != 3) continue;

        printf("  Type (standard/hazardous/reefer/fragile): ");
        if (scanf("%15s", type) != 1) strcpy(type, "standard");

        fprintf(fp, "%s %.2f %.2fx%.2fx%.2f %s\n", id, weight, l, w, h, type);
    }

    fclose(fp);
    print_success("Cargo manifest saved");

    // Offer to run optimization
    printf("\nFiles created successfully!\n");
    printf("  Ship config: %s\n", ship_file);
    printf("  Cargo manifest: %s\n", cargo_file);
    printf("\nRun optimization now? (y/n): ");

    char choice;
    scanf(" %c", &choice);

    if (choice == 'y' || choice == 'Y') {
        ctx->ship_file = ship_file;
        ctx->cargo_file = cargo_file;
        return cmd_optimize(ctx);
    }

    printf("\nTo run optimization manually:\n");
    printf("  cargoforge optimize %s %s\n", ship_file, cargo_file);

    return EXIT_SUCCESS;
}

// SUBCOMMAND: version
int cmd_version(CLIContext *ctx) {
    print_version();
    return EXIT_SUCCESS;
}

// SUBCOMMAND: help
int cmd_help(CLIContext *ctx) {
    if (ctx->ship_file) {
        // Help for specific subcommand
        print_subcommand_help(ctx->ship_file);
    } else {
        print_general_help("cargoforge");
    }
    return EXIT_SUCCESS;
}

// Output results in various formats
void output_results(Ship *ship, AnalysisResult *result, OutputFormat format, const char *output_file) {
    FILE *fp = stdout;

    if (output_file) {
        fp = fopen(output_file, "w");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
            fp = stdout;
        }
    }

    switch (format) {
        case FORMAT_JSON:
            print_json_output(ship, result);
            break;

        case FORMAT_CSV:
            output_csv(ship, result, fp);
            break;

        case FORMAT_TABLE:
            output_table(ship, result, fp);
            break;

        case FORMAT_MARKDOWN:
            output_markdown(ship, result, fp);
            break;

        case FORMAT_HUMAN:
        default:
            print_loading_plan(ship);
            if (g_ctx && g_ctx->show_viz) {
                print_cargo_layout_ascii(ship);
                print_cargo_summary(ship);
            }
            break;
    }

    if (output_file && fp != stdout) {
        fclose(fp);
        if (!g_ctx->quiet) {
            print_success("Results written to file");
        }
    }
}

// Output CSV format
void output_csv(Ship *ship, AnalysisResult *result, FILE *fp) {
    fprintf(fp, "ID,Weight_kg,Length_m,Width_m,Height_m,Type,Placed,Pos_X,Pos_Y,Pos_Z\n");

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        fprintf(fp, "%s,%.2f,%.2f,%.2f,%.2f,%s,%s,%.2f,%.2f,%.2f\n",
                c->id,
                c->weight,
                c->dimensions[0],
                c->dimensions[1],
                c->dimensions[2],
                c->type,
                (c->pos_x >= 0) ? "yes" : "no",
                c->pos_x,
                c->pos_y,
                c->pos_z);
    }
}

// Output table format
void output_table(Ship *ship, AnalysisResult *result, FILE *fp) {
    fprintf(fp, "┌──────────────────┬───────────┬────────────────────┬──────────────┬─────────┬────────────────────────┐\n");
    fprintf(fp, "│ Cargo ID         │ Weight(t) │ Dimensions (LxWxH) │ Type         │ Placed  │ Position (X,Y,Z)       │\n");
    fprintf(fp, "├──────────────────┼───────────┼────────────────────┼──────────────┼─────────┼────────────────────────┤\n");

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];

        // Apply filters if set
        if (g_ctx->only_placed && c->pos_x < 0) continue;
        if (g_ctx->only_failed && c->pos_x >= 0) continue;
        if (g_ctx->cargo_type_filter && strcmp(c->type, g_ctx->cargo_type_filter) != 0) continue;

        char pos[30];
        if (c->pos_x >= 0) {
            snprintf(pos, sizeof(pos), "(%.1f, %.1f, %.1f)", c->pos_x, c->pos_y, c->pos_z);
        } else {
            snprintf(pos, sizeof(pos), "FAILED");
        }

        fprintf(fp, "│ %-16s │ %9.2f │ %.1fx%.1fx%.1f%-7s │ %-12s │ %-7s │ %-22s │\n",
                c->id,
                c->weight / 1000.0,  // Convert to tonnes
                c->dimensions[0],
                c->dimensions[1],
                c->dimensions[2],
                "",
                c->type,
                (c->pos_x >= 0) ? "YES" : "NO",
                pos);
    }

    fprintf(fp, "└──────────────────┴───────────┴────────────────────┴──────────────┴─────────┴────────────────────────┘\n");

    // Summary
    fprintf(fp, "\nSUMMARY:\n");
    fprintf(fp, "  Placed: %d / %d items\n", result->placed_item_count, ship->cargo_count);
    fprintf(fp, "  Total cargo weight: %.2f tonnes\n", result->total_cargo_weight_kg / 1000.0);
    fprintf(fp, "  Center of Gravity: (%.1f%%, %.1f%%)\n", result->cg.perc_x, result->cg.perc_y);
    fprintf(fp, "  Metacentric Height: %.2f m", result->gm);

    if (result->gm < 0.5) {
        fprintf(fp, " [UNSTABLE]\n");
    } else if (result->gm > 2.5) {
        fprintf(fp, " [TOO STIFF]\n");
    } else {
        fprintf(fp, " [OPTIMAL]\n");
    }
}

// Output markdown format
void output_markdown(Ship *ship, AnalysisResult *result, FILE *fp) {
    fprintf(fp, "# CargoForge-C Optimization Report\n\n");

    fprintf(fp, "## Ship Specifications\n\n");
    fprintf(fp, "| Property | Value |\n");
    fprintf(fp, "|----------|-------|\n");
    fprintf(fp, "| Length | %.2f m |\n", ship->length);
    fprintf(fp, "| Width | %.2f m |\n", ship->width);
    fprintf(fp, "| Max Weight | %.0f tonnes |\n", ship->max_weight / 1000.0);
    fprintf(fp, "\n");

    fprintf(fp, "## Cargo Placement Results\n\n");
    fprintf(fp, "| Cargo ID | Weight (t) | Dimensions | Type | Status | Position |\n");
    fprintf(fp, "|----------|------------|------------|------|--------|----------|\n");

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];

        if (g_ctx->only_placed && c->pos_x < 0) continue;
        if (g_ctx->only_failed && c->pos_x >= 0) continue;

        fprintf(fp, "| %s | %.2f | %.1f×%.1f×%.1f | %s | %s | ",
                c->id,
                c->weight / 1000.0,
                c->dimensions[0],
                c->dimensions[1],
                c->dimensions[2],
                c->type,
                (c->pos_x >= 0) ? "✅ Placed" : "❌ Failed");

        if (c->pos_x >= 0) {
            fprintf(fp, "(%.1f, %.1f, %.1f)", c->pos_x, c->pos_y, c->pos_z);
        } else {
            fprintf(fp, "N/A");
        }
        fprintf(fp, " |\n");
    }

    fprintf(fp, "\n## Analysis Summary\n\n");
    fprintf(fp, "- **Placed Items:** %d / %d (%.1f%%)\n",
            result->placed_item_count,
            ship->cargo_count,
            (float)result->placed_item_count / ship->cargo_count * 100.0);
    fprintf(fp, "- **Total Cargo Weight:** %.2f tonnes\n", result->total_cargo_weight_kg / 1000.0);
    fprintf(fp, "- **Capacity Used:** %.1f%%\n",
            result->total_cargo_weight_kg / ship->max_weight * 100.0);
    fprintf(fp, "- **Center of Gravity:** (%.1f%%, %.1f%%)\n", result->cg.perc_x, result->cg.perc_y);
    fprintf(fp, "- **Metacentric Height:** %.2f m ", result->gm);

    if (result->gm < 0.5) {
        fprintf(fp, "⚠️ **UNSTABLE**\n");
    } else if (result->gm > 2.5) {
        fprintf(fp, "⚠️ **TOO STIFF**\n");
    } else {
        fprintf(fp, "✅ **OPTIMAL**\n");
    }

    fprintf(fp, "\n---\n");
    fprintf(fp, "*Generated by CargoForge-C v%s*\n", CARGOFORGE_VERSION);
}

// Output ship info
void output_ship_info(Ship *ship, OutputFormat format) {
    if (format == FORMAT_JSON) {
        printf("{\n");
        printf("  \"ship\": {\n");
        printf("    \"length_m\": %.2f,\n", ship->length);
        printf("    \"width_m\": %.2f,\n", ship->width);
        printf("    \"max_weight_kg\": %.2f,\n", ship->max_weight);
        printf("    \"lightship_weight_kg\": %.2f,\n", ship->lightship_weight);
        printf("    \"lightship_kg_m\": %.2f\n", ship->lightship_kg);
        printf("  }");

        if (ship->cargo_count > 0) {
            printf(",\n  \"cargo_summary\": {\n");
            printf("    \"total_items\": %d,\n", ship->cargo_count);

            float total_weight = 0;
            for (int i = 0; i < ship->cargo_count; i++) {
                total_weight += ship->cargo[i].weight;
            }
            printf("    \"total_weight_kg\": %.2f\n", total_weight);
            printf("  }\n");
        } else {
            printf("\n");
        }
        printf("}\n");
    } else {
        printf("Ship Information\n");
        printf("================\n\n");
        printf("Dimensions:\n");
        printf("  Length: %.2f m\n", ship->length);
        printf("  Width: %.2f m\n", ship->width);
        printf("  Deck Area: %.2f m²\n", ship->length * ship->width);
        printf("\nWeight Capacity:\n");
        printf("  Maximum cargo: %.2f tonnes\n", ship->max_weight / 1000.0);
        printf("  Lightship weight: %.2f tonnes\n", ship->lightship_weight / 1000.0);
        printf("\nStability:\n");
        printf("  Lightship KG: %.2f m\n", ship->lightship_kg);

        if (ship->cargo_count > 0) {
            printf("\nCargo Summary:\n");
            printf("  Total items: %d\n", ship->cargo_count);

            float total_weight = 0;
            int hazardous = 0, reefer = 0, fragile = 0;

            for (int i = 0; i < ship->cargo_count; i++) {
                total_weight += ship->cargo[i].weight;
                if (strcmp(ship->cargo[i].type, "hazardous") == 0) hazardous++;
                if (strcmp(ship->cargo[i].type, "reefer") == 0) reefer++;
                if (strcmp(ship->cargo[i].type, "fragile") == 0) fragile++;
            }

            printf("  Total weight: %.2f tonnes\n", total_weight / 1000.0);
            printf("  Capacity utilization: %.1f%%\n", (total_weight / ship->max_weight) * 100.0);

            if (hazardous > 0) printf("  Hazardous cargo: %d items\n", hazardous);
            if (reefer > 0) printf("  Refrigerated cargo: %d items\n", reefer);
            if (fragile > 0) printf("  Fragile cargo: %d items\n", fragile);
        }
    }
}
