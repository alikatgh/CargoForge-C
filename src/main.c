/*
 * main.c - Entry point for CargoForge-C
 */
#include "cargoforge.h"
#include "cli.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    CLIContext ctx;
    init_cli_context(&ctx);

    int result = parse_cli_args(argc, argv, &ctx);
    if (result <= 0) {
        free_cli_context(&ctx);
        return (result == 0) ? EXIT_SUCCESS : EXIT_INVALID_ARGS;
    }

    int exit_code = dispatch_subcommand(&ctx);
    free_cli_context(&ctx);
    return exit_code;
}
