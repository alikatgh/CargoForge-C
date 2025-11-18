/*
 * cli.h - Command-line interface structures and functions
 *
 * This file defines the modern CLI architecture for CargoForge-C,
 * including subcommands, option parsing, and help systems.
 */

#ifndef CLI_H
#define CLI_H

#include "cargoforge.h"
#include <stdbool.h>

// Version information
#define CARGOFORGE_VERSION "2.0.0"
#define CARGOFORGE_BUILD_DATE __DATE__

// Exit codes for different error types
#define EXIT_SUCCESS 0
#define EXIT_INVALID_ARGS 1
#define EXIT_FILE_ERROR 2
#define EXIT_PARSE_ERROR 3
#define EXIT_OPTIMIZATION_ERROR 4
#define EXIT_VALIDATION_ERROR 5

// Output format types
typedef enum {
    FORMAT_HUMAN,      // Human-readable text
    FORMAT_JSON,       // JSON output
    FORMAT_CSV,        // Comma-separated values
    FORMAT_TABLE,      // ASCII table
    FORMAT_MARKDOWN    // Markdown table
} OutputFormat;

// CLI context - holds all command-line options
typedef struct {
    // Subcommand
    char *subcommand;

    // Input files
    char *ship_file;
    char *cargo_file;
    char *results_file;

    // Output options
    OutputFormat format;
    char *output_file;
    bool verbose;
    bool quiet;
    bool show_viz;
    bool color;

    // Optimization options
    char *algorithm;      // "3d", "2d", "auto"
    int max_iterations;
    int timeout_seconds;

    // Filtering options
    bool only_placed;
    bool only_failed;
    char *cargo_type_filter;

    // Interactive mode
    bool interactive;
} CLIContext;

// Subcommand handler function type
typedef int (*SubcommandHandler)(CLIContext *ctx);

// Subcommand definition
typedef struct {
    const char *name;
    const char *description;
    SubcommandHandler handler;
} Subcommand;

// Function prototypes
// Main CLI functions
int parse_cli_args(int argc, char *argv[], CLIContext *ctx);
void init_cli_context(CLIContext *ctx);
void free_cli_context(CLIContext *ctx);
int dispatch_subcommand(CLIContext *ctx);

// Subcommand handlers
int cmd_optimize(CLIContext *ctx);
int cmd_validate(CLIContext *ctx);
int cmd_info(CLIContext *ctx);
int cmd_analyze(CLIContext *ctx);
int cmd_interactive(CLIContext *ctx);
int cmd_version(CLIContext *ctx);
int cmd_help(CLIContext *ctx);

// Help and usage functions
void print_general_help(const char *prog_name);
void print_subcommand_help(const char *subcommand);
void print_version(void);
void print_examples(void);

// Output formatting functions
void output_results(Ship *ship, AnalysisResult *result, OutputFormat format, const char *output_file);
void output_validation_results(Ship *ship, OutputFormat format);
void output_ship_info(Ship *ship, OutputFormat format);
void output_csv(Ship *ship, AnalysisResult *result, FILE *fp);
void output_table(Ship *ship, AnalysisResult *result, FILE *fp);
void output_markdown(Ship *ship, AnalysisResult *result, FILE *fp);

// Utility functions
bool is_stdin(const char *filename);
const char *get_cargo_type_symbol(const char *type);
const char *get_stability_color(float gm);
void print_progress(const char *message, int percent);
void print_error_with_context(const char *filename, int line, const char *message);
void print_warning(const char *message);
void print_success(const char *message);

// Color codes (ANSI)
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_BOLD    "\x1b[1m"

// Helper macros
#define PRINT_COLOR(color, fmt, ...) \
    if (ctx && ctx->color) { \
        fprintf(stderr, color fmt COLOR_RESET, ##__VA_ARGS__); \
    } else { \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    }

#endif // CLI_H
