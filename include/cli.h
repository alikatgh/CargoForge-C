/*
 * cli.h - Command-line interface structures and functions
 */

#ifndef CLI_H
#define CLI_H

#include "cargoforge.h"
#include <stdbool.h>

#define CARGOFORGE_VERSION "3.0.0"
#define CARGOFORGE_BUILD_DATE __DATE__

/* Exit codes */
#define EXIT_INVALID_ARGS 1
#define EXIT_FILE_ERROR 2
#define EXIT_PARSE_ERROR 3
#define EXIT_OPTIMIZATION_ERROR 4
#define EXIT_VALIDATION_ERROR 5

/* Output formats */
typedef enum {
    FORMAT_HUMAN,
    FORMAT_JSON,
    FORMAT_CSV,
    FORMAT_TABLE,
    FORMAT_MARKDOWN
} OutputFormat;

/* CLI context — holds all parsed options */
typedef struct {
    char *subcommand;
    char *ship_file;
    char *cargo_file;
    OutputFormat format;
    char *output_file;
    bool verbose;
    bool quiet;
    bool show_viz;
    bool color;
    bool only_placed;
    bool only_failed;
    char *cargo_type_filter;
} CLIContext;

/* Core CLI functions */
int  parse_cli_args(int argc, char *argv[], CLIContext *ctx);
void init_cli_context(CLIContext *ctx);
void free_cli_context(CLIContext *ctx);
int  dispatch_subcommand(CLIContext *ctx);

/* Subcommand handlers */
int cmd_optimize(CLIContext *ctx);
int cmd_validate(CLIContext *ctx);
int cmd_info(CLIContext *ctx);
int cmd_version(CLIContext *ctx);
int cmd_help(CLIContext *ctx);

/* Help */
void print_general_help(const char *prog_name);
void print_subcommand_help(const char *subcommand);
void print_version(void);

/* Output formatting */
void output_results(Ship *ship, AnalysisResult *result, OutputFormat format, const char *output_file);
void output_ship_info(Ship *ship, OutputFormat format);
void output_csv(Ship *ship, AnalysisResult *result, FILE *fp);
void output_table(Ship *ship, AnalysisResult *result, FILE *fp);
void output_markdown(Ship *ship, AnalysisResult *result, FILE *fp);

/* Utility */
bool is_stdin(const char *filename);
void print_error_with_context(const char *filename, int line, const char *message);
void print_warning(const char *message);
void print_success(const char *message);

/* ANSI color codes */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_BOLD    "\x1b[1m"

#endif /* CLI_H */
