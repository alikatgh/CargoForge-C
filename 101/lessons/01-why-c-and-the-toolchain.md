# Why C, and the toolchain

CargoForge-C is described in its own README as "a pure C99 maritime cargo loading optimizer... zero external dependencies." Understanding why the project was written in C — and how C source code becomes a running program — is the foundation for everything that follows. This lesson maps the compile-assemble-link-run pipeline onto the files you can actually see in the repository.

## The mental model 🧠

Choosing C is choosing to work one floor above the bare machine — no garbage collector sweeping up behind you, no interpreter standing between your code and the CPU. That is the bargain: you get exact control over every byte and a binary that depends on almost nothing (CargoForge's engine links against only the C standard library and the math library `libm`), and in return you accept responsibility for the memory a higher-level language would have managed for you.

"Compiling" is not one step but an assembly line. Your `.c` text is translated to machine code in `.o` object files, the linker bolts those objects together with `libm` into a single executable, and only then does the operating system load and run it. Almost everything else in this course is a tour of what can go right — and wrong — at each station on that line.

<svg viewBox="0 0 660 170" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:640px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>How C source becomes a running program</title>
<desc>Each .c source file is compiled to a .o object file; the objects are linked with the math library into the cargoforge executable, which the operating system then loads and runs directly.</desc>
<defs><marker id="tc-ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="currentColor" opacity="0.7"/></marker></defs>
<rect x="24" y="52" width="116" height="56" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.7"/>
<text x="82" y="76" fill="currentColor" font-size="13" text-anchor="middle" font-family="var(--md-code-font,monospace)">src/*.c</text>
<text x="82" y="94" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.6">source</text>
<rect x="196" y="52" width="116" height="56" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.7"/>
<text x="254" y="76" fill="currentColor" font-size="13" text-anchor="middle" font-family="var(--md-code-font,monospace)">build/*.o</text>
<text x="254" y="94" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.6">machine code</text>
<rect x="368" y="52" width="124" height="56" rx="6" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.4"/>
<text x="430" y="76" fill="#12A594" font-size="13" text-anchor="middle" font-family="var(--md-code-font,monospace)">cargoforge</text>
<text x="430" y="94" fill="#12A594" font-size="10.5" text-anchor="middle" opacity="0.8">executable</text>
<rect x="548" y="52" width="92" height="56" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.7"/>
<text x="594" y="76" fill="currentColor" font-size="12" text-anchor="middle">stability</text>
<text x="594" y="94" fill="currentColor" font-size="12" text-anchor="middle">report</text>
<line x1="142" y1="80" x2="192" y2="80" stroke="currentColor" stroke-width="1.3" marker-end="url(#tc-ar)"/>
<text x="167" y="72" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.65" font-family="var(--md-code-font,monospace)">cc -c</text>
<line x1="314" y1="80" x2="364" y2="80" stroke="currentColor" stroke-width="1.3" marker-end="url(#tc-ar)"/>
<text x="339" y="72" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.65" font-family="var(--md-code-font,monospace)">cc -o · -lm</text>
<line x1="494" y1="80" x2="544" y2="80" stroke="currentColor" stroke-width="1.3" marker-end="url(#tc-ar)"/>
<text x="519" y="72" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.65" font-family="var(--md-code-font,monospace)">run</text>
<text x="330" y="142" fill="currentColor" font-size="11.5" text-anchor="middle" opacity="0.6">Compiled and linked ahead of time — the OS loads the native binary directly, with no interpreter.</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Compiled language** = "code you write gets translated into machine instructions before it ever runs" — unlike Python or JavaScript, there is no interpreter sitting between CargoForge-C and the CPU at runtime; the OS loads the `cargoforge` binary directly and runs it, which is why hydrostatic loops and GZ integrations can execute thousands of times per second without slowing down.
- **Object file (`.o`)** = "a half-finished puzzle piece of machine code" — when `cc -c src/analysis.c` runs, it turns that one file into `build/analysis.o`, which contains the machine instructions for functions like `perform_analysis` but leaves a blank where any call to a function in another file (say, `parse_cargo_list` in `parser.c`) will eventually go.
- **Linking** = "snapping all the puzzle pieces together into one runnable program" — the linker takes every `.o` in `build/`, resolves the blanks by matching function names to their addresses, appends `-lm` (the math library needed for `sin`, `atan`, `sqrt` in `analysis.c`), and produces the single `cargoforge` executable.
- **`-lm` (the math library flag)** = "tell the linker to include the system's math routines" — without it, the linker would complain about unresolved references to every trigonometric and square-root call in `analysis.c`; it is the only external dependency CargoForge-C needs.
- **`LIB_SRCS` vs `CLI_SRCS`** = "engine vs shell" — the ten engine files (parser, analysis, placement, hydrostatics, etc.) compile into `libcargoforge` and also into the WASM bundle; the four CLI files (`main.c`, `cli.c`, `visualization.c`, `server.c`) are the command-line wrapper that never enters a browser, so Emscripten only sees `LIB_SRCS`.
- **`-fsanitize=address`** = "add a runtime watchdog that catches memory bugs the compiler can't see" — this is the flag that caught the heap-use-after-free in `parse_cargo_list`; it instruments every memory access so that reading freed memory triggers an immediate, descriptive crash instead of silent corruption.

**Why it matters:** if you get the compile–link boundary wrong (e.g., forget to pass an object file to the linker, or omit `-lm`), the build fails with cryptic "undefined reference" errors before a single byte of cargo data is ever processed; understanding this pipeline is what lets you read a Makefile error and know exactly which step broke.

---

## Why C for a Cargo Optimizer?

Ship-stability software has constraints that rule out many popular languages:

**It runs everywhere.** A vessel's onboard computer might be an embedded Linux box from 2010, a standard shore-side PC, or a web browser used by a port agent. C compiles to native machine code on all of them. CargoForge-C takes this literally: the same source tree builds a native CLI binary *and* a WebAssembly module that runs in a browser.

**It has no runtime overhead.** Languages like Python or Java carry a runtime interpreter or virtual machine. C does not. The operating system loads the binary directly. For a tool performing thousands of floating-point calculations per second (hydrostatic interpolation, GZ curve integration, bin-packing), that matters.

**It has no mandatory dependencies.** The README states "zero external dependencies." The only library CargoForge-C links is the standard C math library (`-lm`), which ships with every C toolchain. You can hand the source to any C compiler, anywhere, and build it.

**It is the language of POSIX and naval software.** Legacy stability booklet software, embedded control systems, and IMDG databases are predominantly written in C or C++. A C codebase can directly interface with that ecosystem.

!!! note "C99"
    CargoForge-C targets the C99 standard, declared with `-std=c99`. C99 added `//` comments, `<stdbool.h>`, variable-length array declarations, and guaranteed 64-bit integer types. It is old enough that every compiler supports it, yet modern enough to avoid the quirks of C89.

---

## The Compile → Assemble → Link → Run Pipeline

Writing C is only the first step. Here is what happens between a `.c` file and a running program.

### 1. Preprocessing

Before any real compilation, the C preprocessor (`cpp`) reads your source and expands macros and `#include` directives. When you write:

```c
#include "cargoforge.h"
```

the preprocessor literally pastes the contents of `cargoforge.h` into the `.c` file. `#define` constants such as `MAX_FREE_RECTS` in `cargoforge.h` are replaced with their literal values throughout the file. The output is a single expanded translation unit of pure C text — no macros remain.

### 2. Compilation (and assembly)

The C compiler (`gcc` or `clang`) translates the preprocessed C source into machine instructions. Internally this goes through an assembly stage, but you almost never see the intermediate assembly; the compiler emits an **object file** (`.o`) directly.

An object file contains machine code, but it is not yet runnable. It has unresolved symbols — calls to functions defined in other `.c` files. It is a puzzle piece, not a complete picture.

The Makefile captures this step in one rule (from `Makefile:54`):

```makefile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@
```

- `$(CC)` is `gcc` (defined at line 2).
- `-c` means "compile only — do not link." This produces an object file.
- `-fPIC` means "position-independent code," needed so the same object file can be used in both the static library and the shared library.
- `$<` is the source file; `$@` is the output object file.

Every `.c` file in `src/` produces one `.o` file in `build/`. Run `make` and you will see lines like:

```
gcc -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude -fPIC -c src/analysis.c -o build/analysis.o
```

### 3. Linking

The linker (`ld`, invoked transparently by `gcc`) collects all the object files and resolves the symbols that were previously dangling. When `cli.c` calls `perform_analysis()`, the linker finds that function's machine code in `build/analysis.o` and stitches the addresses together.

The final link step for the CLI binary is (from `Makefile:51–52`):

```makefile
$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(ALL_OBJS) $(LDFLAGS)
```

`$(ALL_OBJS)` is the full list of `.o` files. `$(LDFLAGS)` is `-lm`, which tells the linker to include the system math library (needed for `sin()`, `atan()`, `sqrt()`, used throughout `analysis.c`). The output is the executable `cargoforge`.

### 4. Running

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

The operating system loads the binary into memory, sets up a stack and a heap, and jumps to `main()`. In CargoForge-C, `main.c` is two lines of substance: `init_cli_context` and `parse_cli_args` → `dispatch_subcommand`. Everything else flows from there.

---

## The Compiler Flags in Detail

From `Makefile:4`:

```makefile
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
```

| Flag | Meaning |
|---|---|
| `-O3` | Aggressive optimization. The compiler may reorder instructions, inline functions, and unroll loops. CargoForge-C runs at O3 in production — speed matters for the GZ integration loop. |
| `-Wall -Wextra` | Enable nearly all compiler warnings. A warning in C often points to a real bug. |
| `-std=c99` | Enforce the C99 language standard. Code using C11 or GNU extensions will be rejected. |
| `-D_POSIX_C_SOURCE=200809L` | Expose POSIX.1-2008 extensions in the system headers (`mkstemp`, `strtok_r`, `open_memstream`). Without this define, these functions are hidden. |
| `-Iinclude` | Add `include/` to the header search path, so `#include "cargoforge.h"` resolves without a full path. |

When debugging, the build can switch to `-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g` (from `Makefile:94`). This replaces O3 with debug info and injects runtime checks that catch memory errors and undefined behaviour — the same tooling that found the heap-use-after-free bug in `parse_cargo_list`.

---

## Source Layout: Where the Code Lives

The README project tree maps directly to what `make` does:

```
CargoForge-C/
├── src/        ← all .c source files (compiled to build/*.o)
├── include/    ← all .h header files (included by -Iinclude)
├── build/      ← object files (created by make, ignored by git)
├── tests/      ← 8 test binaries, each linked against relevant .o files
├── examples/   ← sample ship configs and cargo manifests
├── validation/ ← DNV-SE-0475 benchmark binary
└── wasm/       ← Emscripten output (cargoforge.js + cargoforge.wasm)
```

The Makefile divides source into two groups:

- **`LIB_SRCS`** (10 files, `Makefile:15–19`): the pure engine — parser, analysis, placement, constraints, hydrostatics, tanks, longitudinal strength, IMDG, JSON output, and the public library wrapper. These compile into `libcargoforge.a` (static archive) and `libcargoforge.dylib`/`.so` (shared library).
- **`CLI_SRCS`** (4 files, `Makefile:22–23`): `main.c`, `cli.c`, `visualization.c`, `server.c`. These are the shell around the engine. They are not part of the library — an application embedding `libcargoforge` never touches them.

This separation is intentional: the same engine that powers the CLI also compiles to WebAssembly. The `wasm` target (from `Makefile:184–192`) passes `$(LIB_SRCS)` — and only `$(LIB_SRCS)` — to `emcc`, Emscripten's drop-in C compiler:

```makefile
emcc -O3 -std=c99 -Iinclude \
    -s EXPORTED_FUNCTIONS='["_cargoforge_open","_cargoforge_close", ...]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -o wasm/cargoforge.js \
    $(LIB_SRCS)
```

The eight exported functions (`cargoforge_open`, `cargoforge_close`, `cargoforge_load_ship_string`, `cargoforge_load_cargo_string`, `cargoforge_optimize`, `cargoforge_result_json`, `cargoforge_version`, plus `malloc`/`free`) are the public surface of `libcargoforge.h`. The CLI code never enters the WASM bundle.

---

## Your First Build

Clone the repository and run:

```bash
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
make
```

`make` without a target triggers the `all` rule (`Makefile:46`), which builds `build/` and the `cargoforge` binary. To verify everything works:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

To confirm the test suite passes:

```bash
make test
```

Eight test binaries are compiled and run. Each is linked against only the object files it needs — `test_hydrostatics`, for example, links only `build/hydrostatics.o`, not the full engine — so a failure isolates a specific module.

---

## Recap

- CargoForge-C is written in C99 because it needs to run natively on constrained hardware, compile to WebAssembly for the browser, and carry zero runtime dependencies.
- The compile pipeline is: **preprocess** (expand headers and macros) → **compile** (C source to object file) → **link** (combine object files into an executable).
- `-c` tells `gcc`/`clang` to stop after compilation; the linker combines `.o` files into the final binary.
- `CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude` governs every compilation in this project.
- The engine (`LIB_SRCS`) and the CLI (`CLI_SRCS`) are deliberately separate: the same engine object files power the CLI, the static library, the shared library, and the WASM build.
- `make`, `make test`, and `make test-asan` are the three commands you will use most during development.

## Check yourself

??? question "Why C99 instead of a newer C standard, or a different language entirely?"
    CargoForge-C needs zero runtime dependencies and must run natively on constrained hardware and compile to WebAssembly. C99 gives structs, `//` comments, and designated initialisers without pulling in a heavier language runtime or garbage collector.

??? question "What's the actual difference between what `-c` does and what the linker does?"
    `-c` stops the compiler after producing one `.o` object file — a translation with holes wherever it calls a function defined elsewhere. The linker resolves those holes by combining every `.o` file (plus `libm`) into one executable; only then does a function call actually have an address.

??? question "Why does the Makefile keep `LIB_SRCS` and `CLI_SRCS` as separate lists instead of one big list of everything?"
    So the same engine object files can be reused by the CLI, the static library, the shared library, and the WASM build without duplicating the physics code four times.

*Next: [Your first C program](02-your-first-c-program.md).*
