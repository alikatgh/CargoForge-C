# Your first C program

Every C program begins at the same place: a function called `main`. Understanding
what `main` does — and what the operating system does before and after it — unlocks
the structure of every program you will ever read. CargoForge-C is a real-world
example: its [`src/main.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/main.c) is deliberately small so you can see the skeleton without
distraction.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **`main`** = "the one front door every C program must have" — the OS calls it when you run the program; in CargoForge-C it is a 21-line skeleton that immediately hands control to `parse_cli_args` and `dispatch_subcommand` rather than doing any real work itself.
- **`argc` / `argv`** = "the words you typed on the command line, counted and collected" — when you run `cargoforge optimize sample_ship.cfg sample_cargo.txt`, the OS sets `argc = 4` and `argv[2]` / `argv[3]` to the two file paths that every downstream handler depends on.
- **`CLIContext`** = "a single struct that carries everything `main` learned from the command line" — `init_cli_context` fills it with safe defaults, `parse_cli_args` populates it, and `dispatch_subcommand` reads it to route to `cmd_optimize` (which in turn calls `parse_ship_config`, `parse_cargo_list`, `place_cargo_3d`, and `perform_analysis`).
- **exit code** = "a number the program hands back to the shell to say whether it succeeded" — CargoForge-C returns `EXIT_SUCCESS` (0) on success, `EXIT_INVALID_ARGS` on bad arguments, and whatever `dispatch_subcommand` returns for runtime errors; shell scripts, `make`, and CI pipelines read this number to decide whether to continue.
- **`free_cli_context` before every `return`** = "clean up your mess no matter which door you leave through" — `main` calls it in both the early-exit branch and the normal-exit branch; skipping it in either place would leak whatever memory `parse_cli_args` allocated inside `ctx`.
- **`printf` with format specifiers** = "a fill-in-the-blank template for text" — CargoForge-C uses it inside `print_loading_plan` to emit lines like `Draft: 5.23 m`; `%.2f` means "a decimal number rounded to two places," and `\n` ends the line cleanly.

**Why it matters:** if you misread `argc`/`argv` indexing you silently pass the wrong file to `parse_ship_config` or `parse_cargo_list` and get a corrupt loading plan with no error message; if you skip `free_cli_context` in the early-exit path you introduce a memory leak that only shows up under Valgrind — two bugs that look like stability-analysis bugs but live entirely in `main`.

---

## The mental model 🧠

You'll forget the exact syntax — hold THIS picture instead:

> `main` is the harbour-master's office. Every ship that enters or leaves the port must
> check in there first. The office doesn't load cargo itself — it just checks the manifest
> (`argv`), logs the count (`argc`), hands the crew a context sheet (`CLIContext`), and
> waves them toward the right berth (`dispatch_subcommand`). When they're done, it signs
> off on paperwork and closes the gate (`free_cli_context`).

In CargoForge-C, `main` follows that pattern exactly: it reads `argv[1]`–`argv[3]` to
know *which ship and cargo files* arrived, fills a `CLIContext` struct with that
information, and routes to `cmd_optimize` (or `validate`, `info`, etc.) — which is where
`parse_ship_config`, `parse_cargo_list`, `place_cargo_3d`, and `perform_analysis` do the
real heavy lifting.

The office stays small on purpose. Adding a new subcommand means adding a new berth
handler in `cli.c` — the harbour-master's office never changes.

---

<svg viewBox="0 0 620 340" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>CargoForge-C main() call chain</title>
  <desc>Diagram showing how the OS calls main(), which initialises CLIContext, parses argv, and dispatches to cmd_optimize, which calls parse_ship_config, parse_cargo_list, place_cargo_3d, perform_analysis, and output_results.</desc>

  <!-- OS block -->
  <rect x="10" y="20" width="120" height="44" rx="6"
        fill="none" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.5"/>
  <text x="70" y="47" text-anchor="middle" font-size="13" fill="currentColor" opacity="0.6">OS loader</text>

  <!-- arrow OS → main -->
  <line x1="130" y1="42" x2="174" y2="42" stroke="currentColor" stroke-opacity="0.45" stroke-width="1.5" marker-end="url(#arr)"/>
  <text x="150" y="36" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.5">calls</text>

  <!-- main block -->
  <rect x="175" y="10" width="148" height="64" rx="6"
        fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="2"/>
  <text x="249" y="36" text-anchor="middle" font-size="13" font-weight="600" fill="#12A594">main(argc, argv)</text>
  <text x="249" y="54" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.65">init → parse → dispatch</text>
  <text x="249" y="68" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.55">free_cli_context + return</text>

  <!-- CLIContext label -->
  <rect x="175" y="95" width="148" height="36" rx="5"
        fill="none" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2" stroke-dasharray="5 3"/>
  <text x="249" y="116" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.7">CLIContext ctx</text>
  <!-- brace arrow main → CLIContext -->
  <line x1="249" y1="75" x2="249" y2="94" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.2" marker-end="url(#arr-sm)"/>

  <!-- arrow main → dispatch -->
  <line x1="323" y1="42" x2="373" y2="42" stroke="currentColor" stroke-opacity="0.45" stroke-width="1.5" marker-end="url(#arr)"/>
  <text x="348" y="36" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.5">dispatch</text>

  <!-- dispatch / cmd_optimize block -->
  <rect x="374" y="10" width="156" height="64" rx="6"
        fill="none" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.5"/>
  <text x="452" y="33" text-anchor="middle" font-size="12" font-weight="600" fill="currentColor">dispatch_subcommand</text>
  <text x="452" y="51" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.65">→ cmd_optimize(ctx)</text>
  <text x="452" y="65" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.5">(cli.c)</text>

  <!-- arrow cmd_optimize → steps column -->
  <line x1="452" y1="74" x2="452" y2="105" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.3" marker-end="url(#arr-sm)"/>

  <!-- step boxes -->
  <!-- parse_ship_config -->
  <rect x="374" y="106" width="156" height="30" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2"/>
  <text x="452" y="125" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.8">parse_ship_config()</text>

  <line x1="452" y1="136" x2="452" y2="146" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2" marker-end="url(#arr-sm)"/>

  <!-- parse_cargo_list -->
  <rect x="374" y="147" width="156" height="30" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2"/>
  <text x="452" y="166" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.8">parse_cargo_list()</text>

  <line x1="452" y1="177" x2="452" y2="187" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2" marker-end="url(#arr-sm)"/>

  <!-- place_cargo_3d -->
  <rect x="374" y="188" width="156" height="30" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2"/>
  <text x="452" y="207" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.8">place_cargo_3d()</text>

  <line x1="452" y1="218" x2="452" y2="228" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2" marker-end="url(#arr-sm)"/>

  <!-- perform_analysis -->
  <rect x="374" y="229" width="156" height="30" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2"/>
  <text x="452" y="248" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.8">perform_analysis()</text>

  <line x1="452" y1="259" x2="452" y2="269" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2" marker-end="url(#arr-sm)"/>

  <!-- output_results -->
  <rect x="374" y="270" width="156" height="30" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2"/>
  <text x="452" y="289" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.8">output_results()</text>

  <!-- exit code arrow back -->
  <line x1="374" y1="42" x2="323" y2="42" stroke="#D05663" stroke-opacity="0.6" stroke-width="1.3" stroke-dasharray="4 3" marker-end="url(#arr-red)"/>
  <text x="348" y="58" text-anchor="middle" font-size="9" fill="#D05663" opacity="0.75">exit_code</text>

  <!-- argv label on main -->
  <text x="175" y="8" text-anchor="start" font-size="9" fill="currentColor" opacity="0.45">argv[2]=ship.cfg  argv[3]=cargo.txt</text>

  <!-- arrowhead markers -->
  <defs>
    <marker id="arr" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto">
      <path d="M0,0 L8,3 L0,6 Z" fill="currentColor" fill-opacity="0.45"/>
    </marker>
    <marker id="arr-sm" markerWidth="7" markerHeight="5" refX="6" refY="2.5" orient="auto">
      <path d="M0,0 L7,2.5 L0,5 Z" fill="currentColor" fill-opacity="0.3"/>
    </marker>
    <marker id="arr-red" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto">
      <path d="M0,0 L8,3 L0,6 Z" fill="#D05663" fill-opacity="0.6"/>
    </marker>
  </defs>
</svg>

## The entry point

When you run any program on Linux or macOS, the operating system loads the binary,
sets up memory, and then calls one specific function: `main`. Your C code must
define exactly one `main`. Everything else is optional.

Here is the complete [`src/main.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/main.c) for CargoForge-C:

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
| 2 | [`examples/sample_ship.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship.cfg) |
| 3 | [`examples/sample_cargo.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_cargo.txt) |

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
