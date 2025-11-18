/*
 * main.c - Entry point for CargoForge-C.
 *
 * This file is the main entry point for the CargoForge-C CLI application.
 * It initializes the CLI context, parses arguments, and dispatches to
 * the appropriate subcommand handler.
 */
#include "cargoforge.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    CLIContext ctx;
    int result;

    // Initialize CLI context with defaults
    init_cli_context(&ctx);

    // Parse command-line arguments
    result = parse_cli_args(argc, argv, &ctx);

    if (result == 0) {
        // Help or version was displayed, exit successfully
        free_cli_context(&ctx);
        return EXIT_SUCCESS;
    } else if (result < 0) {
        // Error in argument parsing
        free_cli_context(&ctx);
        return EXIT_INVALID_ARGS;
    }

    // Dispatch to appropriate subcommand
    int exit_code = dispatch_subcommand(&ctx);

    // Cleanup
    free_cli_context(&ctx);

    return exit_code;
}

// Legacy usage function for backward compatibility
// (kept in case old code references it, but not used in new CLI)
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <ship_config.cfg> <cargo_list.txt> [options]\n", prog_name);
    fprintf(stderr, "\nThis is the legacy interface. For the new CLI, use:\n");
    fprintf(stderr, "  %s optimize <ship_config.cfg> <cargo_list.txt> [options]\n", prog_name);
    fprintf(stderr, "\nFor full help, use:\n");
    fprintf(stderr, "  %s help\n", prog_name);
}