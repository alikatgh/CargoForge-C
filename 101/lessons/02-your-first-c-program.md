# Your first C program

Every C program begins at the same place: a function called `main`. Understanding
what `main` does — and what the operating system does before and after it — unlocks
the structure of every program you will ever read. CargoForge-C is a real-world
example: its `src/main.c` is deliberately small so you can see the skeleton without
distraction.

---

## The entry point

When you run any program on Linux or macOS, the operating system loads the binary,
sets up memory, and then calls one specific function: `main`. Your C code must
define exactly one `main`. Everything else is optional.

Here is the complete `src/main.c` for CargoForge-C:

```c
/* main.c — Entry point for CargoForge-C */
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
```

Twenty-one lines total, including the comment and blank lines. That is intentional.
`main` should do almost nothing itself; it exists to hand off to the rest of the
program. You will see this pattern in professional C throughout your career.

---

## The signature: `int main(int argc, char *argv[])`

### Return type: `int`

`main` returns an integer. The operating system receives this integer when the
program exits. By convention:

- **0** means success.
- **Any non-zero value** means something went wrong.

Shell scripts, CI pipelines, and the `make` build system all rely on this
convention. When `make test` fails, it is because a test binary returned non-zero.

### `argc` — argument count

`argc` is the number of words typed on the command line, including the program
name itself. When you run:

```
cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

the OS sets `argc = 4`:

| Index | Value |
|-------|-------|
| 0 | `cargoforge` |
| 1 | `optimize` |
| 2 | `examples/sample_ship.cfg` |
| 3 | `examples/sample_cargo.txt` |

### `argv` — argument vector

`argv` is an array of strings (character pointers), one per word. The type
`char *argv[]` means "an array of pointers to characters." You will read this as
"argv is an array of strings."

`argv[0]` is always the program name. `argv[argc]` is always a `NULL` pointer —
a sentinel that marks the end of the array.

!!! note
    `int argc` and `char *argv[]` are parameters, not global variables. The OS
    fills them before calling `main`. You cannot change what was typed, but you
    can read every word.

---

## `EXIT_SUCCESS` and `EXIT_INVALID_ARGS`

The standard header `<stdlib.h>` defines `EXIT_SUCCESS` (0) and `EXIT_FAILURE`
(1). CargoForge-C defines its own `EXIT_INVALID_ARGS` in `cargoforge.h` for the
case where the user typed bad arguments — a more informative exit code than a
generic 1.

The expression on line 15:

```c
return (result == 0) ? EXIT_SUCCESS : EXIT_INVALID_ARGS;
```

is a **ternary operator**: if `result == 0`, return `EXIT_SUCCESS`; otherwise
return `EXIT_INVALID_ARGS`. It is a compact if-else that fits on one line when
the logic is simple.

---

## What `main` actually does

### Step 1 — Initialise the context

```c
CLIContext ctx;
init_cli_context(&ctx);
```

`CLIContext` is a struct (defined in `cli.h`) that holds the parsed subcommand,
file paths, output format, and flags for the current run. Declaring `CLIContext ctx`
creates the struct on the stack. `init_cli_context` fills it with safe defaults —
zeroes, NULLs, and the human-readable output format.

The `&ctx` syntax means "the address of ctx." `init_cli_context` receives a
pointer so it can modify the struct in place. You will see this pattern constantly
in C because functions cannot return large structs cheaply — they operate through
pointers instead.

### Step 2 — Parse the command line

```c
int result = parse_cli_args(argc, argv, &ctx);
if (result <= 0) {
    free_cli_context(&ctx);
    return (result == 0) ? EXIT_SUCCESS : EXIT_INVALID_ARGS;
}
```

`parse_cli_args` reads `argv`, identifies the subcommand (`optimize`, `validate`,
`info`, `serve`, `version`, `help`), stores file paths, and sets flags in `ctx`.
It returns:

- **positive** — parsed successfully, proceed.
- **0** — a help or version request was handled; exit cleanly.
- **negative** — invalid arguments; exit with an error code.

The early return here is a guard clause: get the error cases out of the way
immediately so the rest of the function reads top-to-bottom without nesting.

!!! tip
    `free_cli_context(&ctx)` appears in both exit branches. Releasing resources
    before every return — not just the last one — is a discipline you must build
    from day one. CargoForge-C enforces it here precisely because forgetting it
    causes memory leaks.

### Step 3 — Dispatch to the real work

```c
int exit_code = dispatch_subcommand(&ctx);
free_cli_context(&ctx);
return exit_code;
```

`dispatch_subcommand` reads `ctx.subcommand` and calls the appropriate handler:
`optimize`, `validate`, `info`, `serve`, and so on. Each handler does the heavy
lifting — parsing ship configs, placing cargo, running the stability analysis —
and returns an exit code. `main` passes that code straight back to the OS.

---

## Building and running

From the repository root:

```bash
make
```

The Makefile compiles every `.c` file in `src/` with these flags:

```
-O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
```

`-Wall -Wextra` enable nearly all compiler warnings. Treat warnings as errors
from the start: a warning in C is often a real bug.

Once built, run the optimizer on the bundled example files:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

CargoForge-C will:

1. Parse the ship configuration from `sample_ship.cfg` (length, width, deadweight,
   lightship weight, and so on).
2. Parse the cargo manifest from `sample_cargo.txt` (item IDs, weights, dimensions,
   types).
3. Place cargo into the three hard-coded holds (ForwardHold, AftHold, Deck) using
   the 3-D bin-packing algorithm.
4. Run the stability analysis — draft, KG, GM, GZ curve, IMO criteria.
5. Print a human-readable loading plan to standard output.

All of that work is triggered by the two file-path arguments that `main` received
through `argv[2]` and `argv[3]`.

---

## `printf` — printing to the terminal

CargoForge-C uses `printf` throughout its output routines (for example, inside
`print_loading_plan` in `analysis.c`). The basic form is:

```c
printf("Draft: %.2f m\n", draft);
```

- The first argument is a **format string**: literal text mixed with conversion
  specifiers like `%d` (integer), `%f` (float), `%s` (string), `%.2f` (float to
  2 decimal places).
- Subsequent arguments supply the values for each specifier, in order.
- `\n` is a newline character.

`printf` writes to **standard output** (stdout), which by default appears in the
terminal. The OS connects stdout to the terminal before `main` is called; you do
not have to set it up. Redirect it with `> file.txt` on the command line and the
output goes to a file instead — your C code does not change.

!!! warning
    A missing newline at the end of the last line of output can cause the shell
    prompt to appear on the same line as your output. Always end the final line of
    a block of output with `\n`.

---

## Tracing the full call chain

When you run `cargoforge optimize ...`, execution flows like this:

```
OS loader
  └─ main(argc=4, argv=[...])
       ├─ init_cli_context(&ctx)
       ├─ parse_cli_args(argc, argv, &ctx)   // identifies "optimize"
       └─ dispatch_subcommand(&ctx)
            └─ cmd_optimize(&ctx)            // in cli.c
                 ├─ parse_ship_config(...)   // parser.c
                 ├─ parse_cargo_list(...)    // parser.c
                 ├─ place_cargo_3d(...)      // placement_3d.c
                 ├─ perform_analysis(...)    // analysis.c
                 └─ output_results(...)      // cli.c → json_output.c or print_loading_plan
```

`main.c` stays untouched no matter which subcommand is added. New subcommands go
into `cli.c`; new analysis modules go into their own `.c` files. This separation
is not accidental — it is the standard pattern for keeping a C program navigable
as it grows.

---

## Recap

- Every C program has exactly one `main` function; the OS calls it and receives
  its return value as the program's exit code.
- `argc` is the count of command-line words; `argv` is the array of those words
  as strings, with `argv[0]` being the program name.
- `EXIT_SUCCESS` (0) signals success to the shell; any non-zero value signals
  failure. CargoForge-C defines `EXIT_INVALID_ARGS` for bad argument errors.
- `main` in CargoForge-C does three things only: initialise a context struct,
  parse the CLI arguments, and dispatch to the right subcommand handler.
- `printf` writes formatted text to standard output using conversion specifiers
  (`%d`, `%f`, `%s`, etc.) and escape sequences (`\n`).
- Free every resource before every `return`, not just the last one — both exit
  paths in `main` call `free_cli_context`.

*Next: [Types, control flow, and functions](03-types-control-flow-functions.md).*
