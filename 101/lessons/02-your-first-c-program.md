# Your first C program

Every C program begins at the same place: a function called `main`. Understanding
what `main` does — and what the operating system does before and after it — unlocks
the structure of every program you will ever read. CargoForge-C is a real-world
example: its [`src/main.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/main.c) is deliberately small so you can see the skeleton without
distraction.

## The mental model 🧠

A C program is a building with exactly one front door. The operating system is the only visitor, and it always comes in through `main` — never through any other function directly. On the way in it hands `main` two things: a count of the words you typed (`argc`) and the list of those words (`argv`). `main` does its job, and on the way out hands back a single number — `0` for "all good," anything else for "something went wrong."

CargoForge's [`src/main.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/main.c) is deliberately a *receptionist*, not a worker. It reads the first word after the program name — `optimize`, `validate`, or `serve` — and walks you to the right department. The heavy lifting lives in other files; `main` just routes. That is why it stays small enough to read in one sitting.

Hold this picture: **one door in, one number out.** Every C program you ever open, from a three-line "hello world" to an operating-system kernel, has this exact skeleton underneath.

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>A C program has exactly one entry point: main()</title>
<desc>The operating system calls main with argc and argv. CargoForge's main reads the first argument and dispatches to one subcommand — optimize, validate, or serve — then returns a single exit code to the OS.</desc>
<rect x="150" y="14" width="300" height="28" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.25"/>
<text x="166" y="32" font-size="12.5" font-family="var(--md-code-font,monospace)" fill="currentColor" opacity="0.85">$ cargoforge <tspan fill="#12A594">optimize</tspan> ship.cfg</text>
<text x="300" y="58" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.55">argc = 3   argv = { "cargoforge", "optimize", "ship.cfg" }</text>
<rect x="24" y="96" width="74" height="40" rx="5" fill="currentColor" fill-opacity="0.06" stroke="currentColor" stroke-opacity="0.4"/>
<text x="61" y="121" font-size="12" text-anchor="middle" fill="currentColor">OS</text>
<line x1="98" y1="116" x2="168" y2="116" stroke="currentColor" stroke-opacity="0.5"/>
<path d="M161,112 L168,116 L161,120" fill="none" stroke="currentColor" stroke-opacity="0.6"/>
<text x="133" y="108" font-size="9" text-anchor="middle" fill="currentColor" opacity="0.55">calls</text>
<rect x="168" y="92" width="120" height="48" rx="5" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.2"/>
<text x="228" y="113" font-size="12.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">main(argc,</text>
<text x="228" y="129" font-size="12.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">argv)</text>
<line x1="288" y1="116" x2="378" y2="86" stroke="#12A594" stroke-opacity="0.8"/>
<path d="M371,85 L378,86 L374,92" fill="none" stroke="#12A594"/>
<line x1="288" y1="116" x2="378" y2="116" stroke="currentColor" stroke-opacity="0.25" stroke-dasharray="3 3"/>
<line x1="288" y1="116" x2="378" y2="146" stroke="currentColor" stroke-opacity="0.25" stroke-dasharray="3 3"/>
<rect x="380" y="70" width="104" height="30" rx="4" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.1"/>
<text x="432" y="89" font-size="11.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">optimize</text>
<rect x="380" y="104" width="104" height="26" rx="4" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.3"/>
<text x="432" y="121" font-size="11" text-anchor="middle" fill="currentColor" opacity="0.55" font-family="var(--md-code-font,monospace)">validate</text>
<rect x="380" y="134" width="104" height="26" rx="4" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.3"/>
<text x="432" y="151" font-size="11" text-anchor="middle" fill="currentColor" opacity="0.55" font-family="var(--md-code-font,monospace)">serve</text>
<path d="M228,140 L228,188 L61,188 L61,138" fill="none" stroke="#D05663" stroke-opacity="0.7" stroke-dasharray="4 3"/>
<path d="M57,145 L61,138 L65,145" fill="none" stroke="#D05663" stroke-opacity="0.8"/>
<text x="150" y="206" font-size="10.5" text-anchor="middle" fill="#D05663" opacity="0.9">return 0  →  exit code (0 = success)</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Function** = "a named, reusable block of code that does one job" — you can hand it inputs (its *parameters*), and it can hand back a result (its *return value*). `main` is one function; CargoForge-C is built almost entirely out of small functions like `parse_ship_config` and `perform_analysis` calling each other.
- **Array / pointer / struct** (all used below, all covered properly much later) = for now: an *array* is a numbered row of same-type values sitting back-to-back in memory (Lesson 12); a *pointer* is a variable that stores another variable's address rather than a value itself (Lesson 9); a *struct* is a labelled bundle of related fields treated as one unit (Lesson 5). This lesson uses all three in passing — a quick "you'll get the full picture soon" is enough for now.
- **`main`** = "the one front door every C program must have" — the OS calls it when you run the program; in CargoForge-C it is a 21-line skeleton that immediately hands control to `parse_cli_args` and `dispatch_subcommand` rather than doing any real work itself.
- **`argc` / `argv`** = "the words you typed on the command line, counted and collected" — when you run `cargoforge optimize sample_ship.cfg sample_cargo.txt`, the OS sets `argc = 4` and `argv[2]` / `argv[3]` to the two file paths that every downstream handler depends on.
- **`CLIContext`** = "a single struct that carries everything `main` learned from the command line" — `init_cli_context` fills it with safe defaults, `parse_cli_args` populates it, and `dispatch_subcommand` reads it to route to `cmd_optimize` (which in turn calls `parse_ship_config`, `parse_cargo_list`, `place_cargo_3d`, and `perform_analysis`).
- **exit code** = "a number the program hands back to the shell to say whether it succeeded" — CargoForge-C returns `EXIT_SUCCESS` (0) on success, `EXIT_INVALID_ARGS` on bad arguments, and whatever `dispatch_subcommand` returns for runtime errors; shell scripts, `make`, and CI pipelines read this number to decide whether to continue.
- **`free_cli_context` before every `return`** = "clean up your mess no matter which door you leave through" — `main` calls it in both the early-exit branch and the normal-exit branch; skipping it in either place would leak whatever memory `parse_cli_args` allocated inside `ctx`.
- **`printf` with format specifiers** = "a fill-in-the-blank template for text" — CargoForge-C uses it inside `print_loading_plan` to emit lines like `Draft: 5.23 m`; `%.2f` means "a decimal number rounded to two places," and `\n` ends the line cleanly.

**Why it matters:** if you misread `argc`/`argv` indexing you silently pass the wrong file to `parse_ship_config` or `parse_cargo_list` and get a corrupt loading plan with no error message; if you skip `free_cli_context` in the early-exit path you introduce a memory leak that only shows up under Valgrind — two bugs that look like stability-analysis bugs but live entirely in `main`.

---

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

`argv` is an array of strings (character pointers), one per word — an *array* is
just a numbered row of values back-to-back in memory (Lesson 12 covers this
properly), and a *pointer* stores an address rather than a value directly
(Lesson 9). The type `char *argv[]` means "an array of pointers to characters."
You will read this as "argv is an array of strings" — you do not need the full
mechanics of either concept yet to follow this lesson.

`argv[0]` is always the program name. `argv[argc]` is always a `NULL` pointer —
a sentinel that marks the end of the array.

!!! note
    `int argc` and `char *argv[]` are parameters — the named inputs a function
    declares it needs, listed inside its parentheses — not global variables (a
    global variable would be visible to every function in the program; a
    parameter belongs only to the one function that declared it). The OS
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
file paths, output format, and flags for the current run — a *struct* just
bundles several related fields under one name instead of juggling them as
separate loose variables (Lesson 5 covers this properly). Declaring `CLIContext ctx`
creates the struct on the stack — one of two places C stores data in memory;
for now just know it is automatically cleaned up the moment `main` returns
(Lesson 10 explains exactly what the stack is and why that matters). `init_cli_context` fills it with safe defaults —
zeroes, NULLs, and the human-readable output format.

The `&ctx` syntax means "the address of ctx." `init_cli_context` receives a
pointer — a variable that stores an address instead of a value (Lesson 9) — so it can modify the struct in place. You will see this pattern constantly
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

The early return here is a guard clause — a pattern where you handle a
problem case immediately and exit, rather than wrapping the rest of the
function in a big "if everything's fine" block. Get the error cases out of the way
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

## Check yourself

??? question "What two things does the operating system hand to main() when a program starts, and what does main() hand back?"
    It hands in `argc` (a count of the words typed) and `argv` (the words themselves, as an array of strings); `main` hands back a single integer exit code — 0 for success, anything else for failure.

??? question "Why is src/main.c deliberately small, mostly just reading argv[1] and dispatching?"
    main is a receptionist, not a worker — it routes to the right subcommand function and lets the real physics, parsing, and placement logic live in other files. That is what keeps it readable in one sitting.

*Next: [Types, control flow, and functions](03-types-control-flow-functions.md).*
