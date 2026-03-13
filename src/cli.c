/*
 * cli.c - Command-line interface implementation
 */

#include "cli.h"
#include "placement_3d.h"
#include "visualization.h"
#include "json_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

static CLIContext *g_ctx = NULL;

static void load_config_file(CLIContext *ctx, const char *config_path) {
    FILE *fp = fopen(config_path, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (!key || !value) continue;
        while (*value == ' ' || *value == '\t') value++;

        if (strcmp(key, "format") == 0) {
            if (strcmp(value, "json") == 0) ctx->format = FORMAT_JSON;
            else if (strcmp(value, "csv") == 0) ctx->format = FORMAT_CSV;
            else if (strcmp(value, "table") == 0) ctx->format = FORMAT_TABLE;
            else if (strcmp(value, "markdown") == 0) ctx->format = FORMAT_MARKDOWN;
        }
        else if (strcmp(key, "color") == 0) {
            ctx->color = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
        else if (strcmp(key, "verbose") == 0) {
            ctx->verbose = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
        else if (strcmp(key, "quiet") == 0) {
            ctx->quiet = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
        else if (strcmp(key, "show_viz") == 0) {
            ctx->show_viz = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
    }

    fclose(fp);
}

void init_cli_context(CLIContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->format = FORMAT_HUMAN;
    ctx->show_viz = true;
    ctx->color = isatty(STDERR_FILENO);
    g_ctx = ctx;

    char *home = getenv("HOME");
    if (home) {
        char global_config[512];
        snprintf(global_config, sizeof(global_config), "%s/.cargoforgerc", home);
        load_config_file(ctx, global_config);
    }
    load_config_file(ctx, ".cargoforgerc");
}

void free_cli_context(CLIContext *ctx) {
    (void)ctx;
    g_ctx = NULL;
}

bool is_stdin(const char *filename) {
    return filename && strcmp(filename, "-") == 0;
}

void print_version(void) {
    printf("CargoForge-C version %s\n", CARGOFORGE_VERSION);
    printf("Build date: %s\n", CARGOFORGE_BUILD_DATE);
    printf("Maritime cargo optimization engine\n");
}

void print_general_help(const char *prog_name) {
    printf("CargoForge-C - Maritime Cargo Optimization Engine\n\n");
    printf("USAGE:\n");
    printf("  %s <subcommand> [options]\n\n", prog_name);

    printf("SUBCOMMANDS:\n");
    printf("  optimize    Run 3D bin-packing optimization\n");
    printf("  validate    Validate ship config and cargo manifest\n");
    printf("  info        Display ship and cargo statistics\n");
    printf("  version     Display version information\n");
    printf("  help        Show this help message\n\n");

    printf("GLOBAL OPTIONS:\n");
    printf("  -h, --help           Show help message\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -q, --quiet          Suppress non-essential output\n");
    printf("  --no-color           Disable colored output\n\n");

    printf("EXAMPLES:\n");
    printf("  %s optimize ship.cfg cargo.txt\n", prog_name);
    printf("  %s optimize ship.cfg cargo.txt --format=json\n", prog_name);
    printf("  %s validate ship.cfg cargo.txt\n", prog_name);
    printf("  %s info ship.cfg cargo.txt\n\n", prog_name);
}

void print_subcommand_help(const char *subcommand) {
    if (strcmp(subcommand, "optimize") == 0) {
        printf("cargoforge optimize <ship_config> <cargo_manifest> [options]\n\n");
        printf("OPTIONS:\n");
        printf("  --format=FORMAT      Output: human|json|csv|table|markdown\n");
        printf("  --output=FILE        Write output to file\n");
        printf("  --no-viz             Disable ASCII visualization\n");
        printf("  --only-placed        Show only placed cargo\n");
        printf("  --only-failed        Show only failed cargo\n");
        printf("  --type=TYPE          Filter by cargo type\n");
        printf("  -v, --verbose        Verbose output\n");
    }
    else if (strcmp(subcommand, "validate") == 0) {
        printf("cargoforge validate <ship_config> <cargo_manifest> [options]\n\n");
        printf("OPTIONS:\n");
        printf("  -v, --verbose        Show detailed validation info\n");
    }
    else if (strcmp(subcommand, "info") == 0) {
        printf("cargoforge info <ship_config> [cargo_manifest] [options]\n\n");
        printf("OPTIONS:\n");
        printf("  --format=FORMAT      Output: human|json\n");
    }
    else {
        printf("No help available for: %s\n", subcommand);
    }
}

/* --- Colored output utilities --- */

void print_success(const char *message) {
    if (g_ctx && g_ctx->color)
        fprintf(stderr, COLOR_GREEN "[OK]" COLOR_RESET " %s\n", message);
    else
        fprintf(stderr, "[OK] %s\n", message);
}

void print_error_with_context(const char *filename, int line, const char *message) {
    if (g_ctx && g_ctx->color) {
        fprintf(stderr, COLOR_RED "[ERROR]" COLOR_RESET " %s", filename);
    } else {
        fprintf(stderr, "[ERROR] %s", filename);
    }
    if (line > 0) fprintf(stderr, " (line %d)", line);
    fprintf(stderr, ": %s\n", message);
}

void print_warning(const char *message) {
    if (g_ctx && g_ctx->color)
        fprintf(stderr, COLOR_YELLOW "[WARNING]" COLOR_RESET " %s\n", message);
    else
        fprintf(stderr, "[WARNING] %s\n", message);
}

/* --- Argument parsing --- */

int parse_cli_args(int argc, char *argv[], CLIContext *ctx) {
    if (argc < 2) {
        print_general_help(argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_general_help(argv[0]);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        print_version();
        return 0;
    }

    ctx->subcommand = argv[1];

    static struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"verbose",     no_argument,       0, 'v'},
        {"quiet",       no_argument,       0, 'q'},
        {"format",      required_argument, 0, 'f'},
        {"output",      required_argument, 0, 'o'},
        {"no-viz",      no_argument,       0, 'n'},
        {"no-color",    no_argument,       0, 'c'},
        {"only-placed", no_argument,       0, 'p'},
        {"only-failed", no_argument,       0, 'F'},
        {"type",        required_argument, 0, 't'},
        {"json",        no_argument,       0, 'j'},
        {0, 0, 0, 0}
    };

    optind = 2;
    int opt;
    while ((opt = getopt_long(argc, argv, "hvqf:o:t:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': print_subcommand_help(ctx->subcommand); return 0;
            case 'v': ctx->verbose = true; break;
            case 'q': ctx->quiet = true; break;
            case 'f':
                if (strcmp(optarg, "json") == 0) ctx->format = FORMAT_JSON;
                else if (strcmp(optarg, "csv") == 0) ctx->format = FORMAT_CSV;
                else if (strcmp(optarg, "table") == 0) ctx->format = FORMAT_TABLE;
                else if (strcmp(optarg, "markdown") == 0) ctx->format = FORMAT_MARKDOWN;
                else if (strcmp(optarg, "human") == 0) ctx->format = FORMAT_HUMAN;
                else { fprintf(stderr, "Error: Unknown format '%s'\n", optarg); return -1; }
                break;
            case 'o': ctx->output_file = optarg; break;
            case 'n': ctx->show_viz = false; break;
            case 'c': ctx->color = false; break;
            case 'p': ctx->only_placed = true; break;
            case 'F': ctx->only_failed = true; break;
            case 't': ctx->cargo_type_filter = optarg; break;
            case 'j': ctx->format = FORMAT_JSON; ctx->show_viz = false; break;
            default: return -1;
        }
    }

    if (optind < argc) ctx->ship_file = argv[optind++];
    if (optind < argc) ctx->cargo_file = argv[optind++];

    return 1;
}

/* --- Subcommand dispatch --- */

int dispatch_subcommand(CLIContext *ctx) {
    if (!ctx->subcommand) {
        fprintf(stderr, "Error: No subcommand specified\n");
        return EXIT_INVALID_ARGS;
    }

    if (strcmp(ctx->subcommand, "optimize") == 0) return cmd_optimize(ctx);
    if (strcmp(ctx->subcommand, "validate") == 0) return cmd_validate(ctx);
    if (strcmp(ctx->subcommand, "info") == 0)     return cmd_info(ctx);
    if (strcmp(ctx->subcommand, "version") == 0)  return cmd_version(ctx);
    if (strcmp(ctx->subcommand, "help") == 0)     return cmd_help(ctx);

    fprintf(stderr, "Error: Unknown subcommand '%s'\n", ctx->subcommand);
    return EXIT_INVALID_ARGS;
}

/* --- SUBCOMMAND: optimize --- */

int cmd_optimize(CLIContext *ctx) {
    if (!ctx->ship_file || !ctx->cargo_file) {
        fprintf(stderr, "Usage: cargoforge optimize <ship_config> <cargo_manifest> [options]\n");
        return EXIT_INVALID_ARGS;
    }

    Ship ship = {0};

    if (ctx->verbose) fprintf(stderr, "Parsing ship configuration...\n");
    if (parse_ship_config(ctx->ship_file, &ship) != 0) {
        print_error_with_context(ctx->ship_file, 0, "Failed to parse ship configuration");
        return EXIT_PARSE_ERROR;
    }
    if (!ctx->quiet) print_success("Ship configuration loaded");

    if (ctx->verbose) fprintf(stderr, "Parsing cargo manifest...\n");
    if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
        print_error_with_context(ctx->cargo_file, 0, "Failed to parse cargo manifest");
        free(ship.cargo);
        return EXIT_PARSE_ERROR;
    }
    if (!ctx->quiet) print_success("Cargo manifest loaded");

    if (!ctx->quiet) fprintf(stderr, "\nRunning 3D bin-packing...\n");
    place_cargo_3d(&ship);
    if (!ctx->quiet) print_success("Optimization complete");

    AnalysisResult result = perform_analysis(&ship);
    output_results(&ship, &result, ctx->format, ctx->output_file);

    free(ship.cargo);
    return EXIT_SUCCESS;
}

/* --- SUBCOMMAND: validate --- */

int cmd_validate(CLIContext *ctx) {
    if (!ctx->ship_file || !ctx->cargo_file) {
        fprintf(stderr, "Usage: cargoforge validate <ship_config> <cargo_manifest>\n");
        return EXIT_INVALID_ARGS;
    }

    Ship ship = {0};
    int errors = 0;

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

    fprintf(stderr, "\nValidating cargo manifest: %s\n", ctx->cargo_file);
    if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
        print_error_with_context(ctx->cargo_file, 0, "Invalid cargo manifest");
        errors++;
    } else {
        print_success("Cargo manifest is valid");
        if (ctx->verbose) {
            printf("  Total items: %d\n", ship.cargo_count);
            float total_weight = 0;
            for (int i = 0; i < ship.cargo_count; i++)
                total_weight += ship.cargo[i].weight;
            printf("  Total weight: %.0f kg\n", total_weight);
            if (total_weight > ship.max_weight)
                print_warning("Total cargo weight exceeds ship capacity!");
        }
    }

    fprintf(stderr, "\n");
    if (errors == 0) {
        print_success("All validation checks passed!");
        if (ship.cargo) free(ship.cargo);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "[FAILED] Validation failed with %d error(s)\n", errors);
    if (ship.cargo) free(ship.cargo);
    return EXIT_VALIDATION_ERROR;
}

/* --- SUBCOMMAND: info --- */

int cmd_info(CLIContext *ctx) {
    if (!ctx->ship_file) {
        fprintf(stderr, "Usage: cargoforge info <ship_config> [cargo_manifest]\n");
        return EXIT_INVALID_ARGS;
    }

    Ship ship = {0};

    if (parse_ship_config(ctx->ship_file, &ship) != 0) {
        print_error_with_context(ctx->ship_file, 0, "Failed to parse ship configuration");
        return EXIT_PARSE_ERROR;
    }

    if (ctx->cargo_file) {
        if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
            print_error_with_context(ctx->cargo_file, 0, "Failed to parse cargo manifest");
            free(ship.cargo);
            return EXIT_PARSE_ERROR;
        }
    }

    output_ship_info(&ship, ctx->format);
    if (ship.cargo) free(ship.cargo);
    return EXIT_SUCCESS;
}

/* --- SUBCOMMAND: version / help --- */

int cmd_version(CLIContext *ctx) {
    (void)ctx;
    print_version();
    return EXIT_SUCCESS;
}

int cmd_help(CLIContext *ctx) {
    if (ctx->ship_file)
        print_subcommand_help(ctx->ship_file);
    else
        print_general_help("cargoforge");
    return EXIT_SUCCESS;
}

/* --- Output formatting --- */

void output_results(Ship *ship, AnalysisResult *result, OutputFormat format, const char *output_file) {
    FILE *fp = stdout;
    FILE *old_stdout = NULL;

    if (output_file) {
        fp = fopen(output_file, "w");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
            fp = stdout;
        } else if (format == FORMAT_JSON) {
            old_stdout = stdout;
            stdout = fp;
        }
    }

    switch (format) {
        case FORMAT_JSON:
            print_json_output(ship, result);
            if (old_stdout) { fflush(stdout); stdout = old_stdout; }
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
        if (g_ctx && !g_ctx->quiet)
            print_success("Results written to file");
    }
}

void output_csv(Ship *ship, AnalysisResult *result, FILE *fp) {
    (void)result;
    fprintf(fp, "ID,Weight_kg,Length_m,Width_m,Height_m,Type,Placed,Pos_X,Pos_Y,Pos_Z\n");
    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        fprintf(fp, "%s,%.2f,%.2f,%.2f,%.2f,%s,%s,%.2f,%.2f,%.2f\n",
                c->id, c->weight,
                c->dimensions[0], c->dimensions[1], c->dimensions[2],
                c->type, (c->pos_x >= 0) ? "yes" : "no",
                c->pos_x, c->pos_y, c->pos_z);
    }
}

void output_table(Ship *ship, AnalysisResult *result, FILE *fp) {
    fprintf(fp, "%-16s %9s %18s %-12s %7s %22s\n",
            "Cargo ID", "Weight(t)", "Dimensions (LxWxH)", "Type", "Placed", "Position (X,Y,Z)");
    fprintf(fp, "---------------- --------- ------------------ ------------ ------- ----------------------\n");

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        if (g_ctx && g_ctx->only_placed && c->pos_x < 0) continue;
        if (g_ctx && g_ctx->only_failed && c->pos_x >= 0) continue;
        if (g_ctx && g_ctx->cargo_type_filter && strcmp(c->type, g_ctx->cargo_type_filter) != 0) continue;

        char pos[30];
        if (c->pos_x >= 0)
            snprintf(pos, sizeof(pos), "(%.1f, %.1f, %.1f)", c->pos_x, c->pos_y, c->pos_z);
        else
            snprintf(pos, sizeof(pos), "-");

        fprintf(fp, "%-16s %9.2f %.1fx%.1fx%.1f%7s %-12s %7s %22s\n",
                c->id, c->weight / 1000.0f,
                c->dimensions[0], c->dimensions[1], c->dimensions[2], "",
                c->type, (c->pos_x >= 0) ? "YES" : "NO", pos);
    }

    fprintf(fp, "\nPlaced: %d / %d | Weight: %.2f t | GM: %.2f m\n",
            result->placed_item_count, ship->cargo_count,
            result->total_cargo_weight_kg / 1000.0f, result->gm);
}

void output_markdown(Ship *ship, AnalysisResult *result, FILE *fp) {
    fprintf(fp, "# CargoForge-C Optimization Report\n\n");

    fprintf(fp, "## Ship Specifications\n\n");
    fprintf(fp, "| Property | Value |\n|----------|-------|\n");
    fprintf(fp, "| Length | %.2f m |\n", ship->length);
    fprintf(fp, "| Width | %.2f m |\n", ship->width);
    fprintf(fp, "| Max Weight | %.0f t |\n\n", ship->max_weight / 1000.0f);

    fprintf(fp, "## Cargo Placement\n\n");
    fprintf(fp, "| ID | Weight (t) | Dims | Type | Status | Position |\n");
    fprintf(fp, "|----|------------|------|------|--------|----------|\n");

    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        fprintf(fp, "| %s | %.2f | %.1fx%.1fx%.1f | %s | %s | ",
                c->id, c->weight / 1000.0f,
                c->dimensions[0], c->dimensions[1], c->dimensions[2],
                c->type, (c->pos_x >= 0) ? "Placed" : "Failed");
        if (c->pos_x >= 0)
            fprintf(fp, "(%.1f, %.1f, %.1f)", c->pos_x, c->pos_y, c->pos_z);
        else
            fprintf(fp, "N/A");
        fprintf(fp, " |\n");
    }

    fprintf(fp, "\n## Stability Analysis\n\n");
    fprintf(fp, "- **Placed:** %d / %d (%.1f%%)\n",
            result->placed_item_count, ship->cargo_count,
            (float)result->placed_item_count / ship->cargo_count * 100.0f);
    fprintf(fp, "- **Cargo Weight:** %.2f t\n", result->total_cargo_weight_kg / 1000.0f);
    fprintf(fp, "- **CG:** (%.1f%%, %.1f%%)\n", result->cg.perc_x, result->cg.perc_y);
    fprintf(fp, "- **GM:** %.2f m\n", result->gm);
    fprintf(fp, "- **Trim:** %.3f m\n", result->trim);
    fprintf(fp, "- **Heel:** %.2f deg\n", result->heel);
    fprintf(fp, "- **IMO Compliant:** %s\n", result->imo_compliant ? "Yes" : "No");
    fprintf(fp, "\n---\n*Generated by CargoForge-C v%s*\n", CARGOFORGE_VERSION);
}

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
            for (int i = 0; i < ship->cargo_count; i++)
                total_weight += ship->cargo[i].weight;
            printf("    \"total_weight_kg\": %.2f\n", total_weight);
            printf("  }\n");
        } else {
            printf("\n");
        }
        printf("}\n");
    } else {
        printf("Ship Information\n================\n\n");
        printf("Dimensions:\n");
        printf("  Length: %.2f m\n", ship->length);
        printf("  Width: %.2f m\n", ship->width);
        printf("  Deck Area: %.2f m2\n", ship->length * ship->width);
        printf("\nWeight:\n");
        printf("  Max cargo: %.2f t\n", ship->max_weight / 1000.0f);
        printf("  Lightship: %.2f t\n", ship->lightship_weight / 1000.0f);
        printf("  Lightship KG: %.2f m\n", ship->lightship_kg);

        if (ship->cargo_count > 0) {
            printf("\nCargo Summary:\n");
            printf("  Items: %d\n", ship->cargo_count);
            float total_weight = 0;
            int hazardous = 0, reefer = 0, fragile = 0;
            for (int i = 0; i < ship->cargo_count; i++) {
                total_weight += ship->cargo[i].weight;
                if (strcmp(ship->cargo[i].type, "hazardous") == 0) hazardous++;
                if (strcmp(ship->cargo[i].type, "reefer") == 0) reefer++;
                if (strcmp(ship->cargo[i].type, "fragile") == 0) fragile++;
            }
            printf("  Total weight: %.2f t\n", total_weight / 1000.0f);
            printf("  Utilization: %.1f%%\n", (total_weight / ship->max_weight) * 100.0f);
            if (hazardous > 0) printf("  Hazardous: %d\n", hazardous);
            if (reefer > 0)    printf("  Reefer: %d\n", reefer);
            if (fragile > 0)   printf("  Fragile: %d\n", fragile);
        }
    }
}
