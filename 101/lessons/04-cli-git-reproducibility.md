# CLI, git, and reproducible builds

Every program you write eventually has to be compiled, versioned, tested, and shipped — in a way that anyone can reproduce exactly. This lesson walks through how CargoForge-C handles all three: the shell commands you use day-to-day, the [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) that automates building, and the GitHub Actions workflow that re-runs those same steps on every change. Understanding this infrastructure is what separates "code that works on my machine" from code you can trust in production.

## The mental model 🧠

A reproducible build is a recipe so precise that any kitchen produces the *identical* cake. "Works on my machine" is the opposite — a recipe that says "bake until done," secretly depending on your oven, your altitude, your particular bag of flour.

CargoForge removes the guesswork. **Git** is the lab notebook: every change stamped with who, when, and exactly what. The **compiler flags** pinned in the [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) — `-std=c99 -D_POSIX_C_SOURCE=200809L` — are the exact oven temperature and bake time. Because the dialect of C and the library level are nailed down, the same source compiled on your laptop and on the CI runner produces a binary that behaves identically: a port authority's server computes the same GM you do.

The payoff is **trust**. When a result looks wrong you can reproduce it exactly, walk the git history back to the commit that broke it, and know the fix is real — not an accident of one machine's environment.

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>A reproducible build: identical inputs yield an identical binary anywhere</title>
<desc>The same source at one git commit plus the same pinned compiler flags and toolchain, built on a laptop and on CI, produce byte-for-byte the same binary, verified by matching SHA-256 hashes.</desc>
<rect x="16" y="40" width="190" height="44" rx="5" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.1"/>
<text x="111" y="60" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">source @ git a1b2c3d</text>
<text x="111" y="76" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.55">exact same commit</text>
<rect x="16" y="132" width="190" height="44" rx="5" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.1"/>
<text x="111" y="152" font-size="10.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">-std=c99  -O2  -Wall</text>
<text x="111" y="168" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.55">pinned flags + toolchain</text>
<rect x="266" y="36" width="120" height="46" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/>
<text x="326" y="63" font-size="11.5" text-anchor="middle" fill="currentColor">your laptop</text>
<rect x="266" y="134" width="120" height="46" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/>
<text x="326" y="161" font-size="11.5" text-anchor="middle" fill="currentColor">CI runner</text>
<line x1="206" y1="62" x2="264" y2="59" stroke="currentColor" stroke-opacity="0.45"/><path d="M257,55 L264,59 L257,63" fill="none" stroke="currentColor" stroke-opacity="0.55"/>
<line x1="206" y1="154" x2="264" y2="157" stroke="currentColor" stroke-opacity="0.45"/><path d="M257,153 L264,157 L257,161" fill="none" stroke="currentColor" stroke-opacity="0.55"/>
<line x1="206" y1="70" x2="264" y2="150" stroke="currentColor" stroke-opacity="0.16" stroke-dasharray="3 3"/>
<line x1="206" y1="146" x2="264" y2="66" stroke="currentColor" stroke-opacity="0.16" stroke-dasharray="3 3"/>
<line x1="386" y1="59" x2="452" y2="100" stroke="currentColor" stroke-opacity="0.45"/><path d="M445,97 L452,100 L446,105" fill="none" stroke="currentColor" stroke-opacity="0.55"/>
<line x1="386" y1="157" x2="452" y2="116" stroke="currentColor" stroke-opacity="0.45"/><path d="M446,111 L452,116 L445,119" fill="none" stroke="currentColor" stroke-opacity="0.55"/>
<rect x="454" y="84" width="132" height="48" rx="5" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.2"/>
<text x="520" y="104" font-size="11.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">cargoforge</text>
<text x="520" y="120" font-size="9.5" text-anchor="middle" fill="#12A594">sha256 ✓ identical</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Subcommand** = "the first word you give the program that tells it what job to do" — `./cargoforge optimize` picks the stowage-and-stability path, while `validate`, `info`, or `serve` each activate a completely different code path inside `main.c` and `cli.c`.
- **Makefile** = "a recipe book that only cooks the dishes whose ingredients have changed" — instead of retyping the full `gcc` incantation for all ten source files, you run `make` and it timestamps each `.o` against its `.c`; only files that changed since last time are recompiled.
- **Compiler flags (`-std=c99 -D_POSIX_C_SOURCE=200809L`)** = "a contract that locks the dialect of C the project speaks" — any machine with a conforming C99 compiler and a POSIX.1-2008 library will produce the same binary from the same source, which is why a stability calculation gives the same answer on a port authority's server as on your laptop.
- **Deterministic build** = "the compiler, given the same inputs and flags, always produces bit-for-bit identical output" — CargoForge-C achieves this by pinning the C standard, the POSIX level, and writing every `.c` file by hand with no generated or time-dependent code, so the physics results never drift between environments.
- **git commit** = "a permanent, labelled snapshot of exactly what the code looked like at one moment, plus a message explaining why" — the lesson uses the use-after-free fix (`fix: null cargo pointer on parse error`) as a concrete example of a commit message that tells the *why*, not just the *what*.
- **Continuous Integration (CI)** = "a robot that rebuilds and smoke-tests your code on a clean machine every time you push" — the `.github/workflows/c-build.yml` workflow runs `make` and then `./cargoforge examples/...` on a fresh Ubuntu VM, so a change that only worked because of a leftover file on your laptop gets caught before it can reach `main`.
- **`make test-asan`** = "rebuild everything with memory-error detectors switched on, then run the test suite" — AddressSanitizer and UBSan are the tools that catch the kind of heap-use-after-free bug described elsewhere in the course; the `clean` step inside the target is mandatory because sanitizer-compiled `.o` files cannot be mixed with ordinary ones.

**Why it matters:** if the build is not deterministic and the CI not clean, a stability number that looks correct on a developer's laptop can silently differ on the vessel's navigation system — and in cargo stowage, a wrong GM or free-surface calculation is a safety defect, not just a test failure.

---

## The shell and the binary

CargoForge-C compiles to a single executable called `cargoforge`. Once built, you run it from the terminal (also called a shell or command line). A shell is a text interface to your operating system: you type a command, press Enter, and the OS executes it.

The most basic interaction looks like this:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

Breaking this down:

- `./cargoforge` — the `./` prefix means "look in the current directory for a file named `cargoforge` and run it". Without `./`, the shell would search the system-wide `PATH` instead.
- `optimize` — a **subcommand**. CargoForge-C uses the same pattern as tools like `git` or `docker`: the first argument selects what the program does. Other subcommands are `validate`, `info`, `serve`, `version`, and `help`.
- [`examples/sample_ship.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship.cfg) and [`examples/sample_cargo.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_cargo.txt) — arguments to the subcommand: the ship configuration file and the cargo manifest.

Other output formats are available via a flag:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt --format json
```

Valid formats are `human` (default), `json`, `csv`, `table`, and `markdown`. The `json` format is what the HTTP server and library consumers receive.

!!! note "Stdin as input"
    Both the ship config and cargo manifest parsers accept `"-"` as the filename, which means "read from standard input." This lets you pipe data without touching the filesystem:
    ```bash
    cat examples/sample_cargo.txt | ./cargoforge optimize examples/sample_ship.cfg -
    ```

---

## What `make` does

Compiling a C program by hand means invoking `gcc` with a list of source files, flags, and output names. For a project with ten source files and eight test binaries, that list becomes unmanageable — and you'd have to recompile everything even if only one file changed. `make` solves both problems.

The [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) at the root of CargoForge-C is a set of **rules**. Each rule says: "to produce target X, you need files Y and Z; if any of them are newer than X, run this command." `make` compares timestamps and only recompiles what changed.

### The compiler flags

Every object file and binary is compiled with the same flags, declared at the top of the [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile):

```makefile
CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS = -lm
```

What each flag does:

| Flag | Meaning |
|------|---------|
| `-O3` | Optimize aggressively for speed (three levels above none) |
| `-Wall -Wextra` | Enable a broad set of compiler warnings; warnings are treated as useful diagnostics |
| `-std=c99` | Require strict C99 conformance — no compiler-specific extensions |
| `-D_POSIX_C_SOURCE=200809L` | Expose POSIX.1-2008 functions (like `strtok_r`, `open_memstream`) |
| `-Iinclude` | Add the `include/` directory to the header search path |
| `-lm` | Link the math library (`libm`) for `sin`, `cos`, `atan`, etc. |

The `-std=c99` and `-D_POSIX_C_SOURCE` flags together define a precise dialect of C. Any code that compiles under these flags will compile identically on any conforming C99 implementation — Linux, macOS, or Windows with a POSIX layer. That is what "portable" means in practice.

### Source file groups

The [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) splits sources into two groups:

```makefile
LIB_SRCS = $(SRC_DIR)/parser.c $(SRC_DIR)/analysis.c $(SRC_DIR)/placement_3d.c \
           $(SRC_DIR)/constraints.c $(SRC_DIR)/json_output.c \
           $(SRC_DIR)/hydrostatics.c $(SRC_DIR)/tanks.c \
           $(SRC_DIR)/longitudinal_strength.c $(SRC_DIR)/imdg.c \
           $(SRC_DIR)/libcargoforge.c

CLI_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/cli.c $(SRC_DIR)/visualization.c \
           $(SRC_DIR)/server.c
```

`LIB_SRCS` is the pure engine — the stability physics, parser, placement algorithm, and public API. It has no `main()` function and no terminal output. `CLI_SRCS` is the thin shell that wraps it: argument parsing, output formatting, the HTTP server. This separation is intentional: the same `LIB_SRCS` are compiled into `libcargoforge.a` and `libcargoforge.dylib`/`.so` for use by other programs, without dragging in any CLI behaviour.

### The main targets

```makefile
all: $(BUILD_DIR) $(TARGET)

$(TARGET): $(ALL_OBJS)
    $(CC) $(CFLAGS) -o $(TARGET) $(ALL_OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
    $(CC) $(CFLAGS) -fPIC -c $< -o $@
```

The pattern rule `$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c` compiles any `.c` file in `src/` to a `.o` (object file) in `build/`. The `-fPIC` flag ("position-independent code") is required for shared libraries; it costs nothing for a binary.

The `$(TARGET)` rule then **links** all the object files into the final executable. Linking is the step where all the separately compiled pieces are joined together and external libraries (like `-lm`) are resolved.

### Platform detection

```makefile
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  SHARED_EXT = dylib
  SHARED_FLAGS = -dynamiclib -install_name @rpath/libcargoforge.$(SHARED_EXT)
else
  SHARED_EXT = so
  SHARED_FLAGS = -shared -Wl,-soname,libcargoforge.$(SHARED_EXT)
endif
```

macOS uses `.dylib` for shared libraries; Linux uses `.so`. The [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) queries the OS at build time with `$(shell uname -s)` and sets the right extension automatically. This is a simple but real example of cross-platform portability handled in the build system rather than the source code.

### Other useful targets

| Target | What it does |
|--------|--------------|
| `make` | Build `cargoforge` (the default `all` target) |
| `make lib` | Build `libcargoforge.a` and the shared library only |
| `make test` | Build and run all 8 test binaries |
| `make test-asan` | Rebuild with AddressSanitizer + UBSan, then run tests |
| `make test-valgrind` | Run each test binary under Valgrind memory checker |
| `make fuzz` | Run the random-input fuzzer ([`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) |
| `make clean` | Delete `build/`, the binary, libs, and all test binaries |
| `make install` | Copy binary, libs, and header to `/usr/local` (configurable via `PREFIX`) |
| `make validate` | Run the DNV-SE-0475 benchmark vessel validation |
| `make wasm` | Compile to WebAssembly with Emscripten (`emcc` required) |

!!! tip "Always run `make clean` before a sanitizer build"
    The `test-asan` target does this for you — notice the rule: `test-asan: clean all test`. This is because object files compiled without sanitizer flags cannot be mixed with ones compiled with them. A partial rebuild would silently skip the sanitizer on older files.

---

## git basics

`git` is a version control system. It records every change to your code as a **commit** — a snapshot with a unique ID (a 40-character hash), a message, an author, and a timestamp. A collection of commits forms a **repository**.

The most important commands:

```bash
git status          # what files have changed since the last commit?
git diff            # what exactly changed in those files?
git add src/parser.c  # stage a file (mark it for inclusion in the next commit)
git commit -m "fix: null-out cargo pointer on parse error"
git log --oneline   # one-line summary of recent commits
git push            # send commits to the remote repository (GitHub)
```

### Why commit messages matter

A commit message is a permanent record of *why* a change was made. The fix described in the digest — nulling out `ship->cargo` after freeing it to prevent a heap-use-after-free — belongs in a commit message, not just a code comment. Future developers (including your future self) will read it and understand the intent without reconstructing it from the diff.

A common convention: start with a short prefix that classifies the change. For example:
- `fix:` — a bug fix
- `feat:` — a new feature
- `test:` — adding or changing tests
- `docs:` — documentation only

### Branches and pull requests

When you want to work on a new feature or a fix without disturbing the main history, you create a **branch** — a parallel line of commits. When the work is ready, you open a **pull request** (PR) on GitHub, which asks for the branch to be merged into `main`. This is where CI runs (see next section).

---

## Why deterministic builds matter

"Deterministic" means: given the same source code and the same flags, the compiler always produces the same binary. CargoForge-C achieves this by:

1. **Pinning the C standard.** `-std=c99` excludes any GCC-specific behaviour that might vary between versions.
2. **Explicit POSIX level.** `-D_POSIX_C_SOURCE=200809L` is the same constant on any POSIX-conformant system. If this were unset, different Linux distributions might expose different sets of functions.
3. **No generated code.** There is no code-generation step — every `.c` file is hand-written and committed. Nothing in the build is random or time-dependent.

This matters for safety-critical software. If a stability calculation gives a different answer on the port authority's server than on your laptop, you cannot trust either. Determinism is the foundation of correctness.

---

## Continuous Integration (CI)

Continuous Integration (CI) means: every time someone pushes code or opens a pull request, an automated system checks whether the code still builds and passes tests. If it does not, the PR is blocked.

CargoForge-C uses GitHub Actions. The workflow is defined in `.github/workflows/c-build.yml`:

```yaml
name: C/C++ CI

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - name: Check out code
        uses: actions/checkout@v4

      - name: Compile the project
        run: make

      - name: Run a basic test
        run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
```

### Reading the workflow

- **`on:`** — when to run. This workflow runs on every push to `main` and on every PR that targets `main`.
- **`jobs: build:`** — a single job named `build`. A job is a sequence of steps that runs on one machine.
- **`runs-on: ubuntu-latest`** — GitHub provides a fresh Ubuntu virtual machine for each run. "Fresh" matters: the machine has no state from previous runs, so if something only works because a file was left over from last time, CI will catch it.
- **`actions/checkout@v4`** — a pre-built action that clones the repository into the VM.
- **`run: make`** — exactly the same command you type locally. If the compiler flags, source list, or any header has a problem, this step fails and the PR is blocked.
- **`run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt`** — a smoke test: run the binary on the sample files and verify it exits cleanly (exit code 0). A non-zero exit code fails the step.

### What CI cannot replace

The CI workflow above is a minimal smoke test — it verifies compilation and a basic end-to-end run, but it does not run the eight unit test binaries (`make test`), the memory-safety suite (`make test-asan`), or the benchmark validation (`make validate`). Those are available locally and can be added to the workflow as additional steps. The lesson: CI is only as thorough as the commands you put in it.

!!! warning "The environment is always clean"
    Local development accumulates state: cached binaries, leftover files, environment variables set in your shell profile. CI starts clean every time. A build that passes locally but fails in CI almost always means the local build relied on something not committed to the repository — a missing `make clean`, an untracked file, or a system library that is not standard.

---

## Putting it together: a complete workflow

Here is what happens from a code change to a merged PR:

1. You edit [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) to fix a bug.
2. `make test-asan` — rebuild with sanitizers, run all 8 tests. Fix any failures.
3. `git add src/parser.c` — stage the changed file only.
4. `git commit -m "fix: null cargo pointer on parse error to prevent UAF"` — commit with a clear message.
5. `git push origin my-fix-branch` — push to GitHub.
6. Open a pull request. GitHub Actions automatically starts the CI job.
7. CI runs `make` then `./cargoforge examples/...` on a clean Ubuntu VM.
8. If both steps pass, the PR shows a green check. A reviewer can then merge it to `main`.

---

## Recap

- `cargoforge` is invoked as `./cargoforge <subcommand> <ship_cfg> <cargo_manifest>`, with optional `--format` for output style.
- The [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) encodes the complete build recipe: compiler, flags, source groups, test binaries, and platform detection. `make` only recompiles what changed.
- `-std=c99 -D_POSIX_C_SOURCE=200809L` pins the language dialect exactly, making the build portable and deterministic across systems.
- `git` records every change as a committed snapshot with a message; branches and pull requests gate changes through review and CI.
- The GitHub Actions workflow in `.github/workflows/c-build.yml` re-runs `make` and a smoke test on a clean Ubuntu VM on every push and PR, catching regressions before they reach `main`.
- CI is only as thorough as its step list — `make test`, `make test-asan`, and `make validate` are available but must be added explicitly to gain their protection in CI.

*Next: [Lab 1 — Build and Run It](lab-01-foundations.md).*
