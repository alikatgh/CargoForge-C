# Make and CMake

CargoForge-C has fourteen source files, ten headers, eight test binaries, two library outputs, and a WebAssembly build. Compiling all of that by hand — and knowing which pieces to recompile when a single header changes — is exactly the problem that build systems solve. This lesson explains how Make works from first principles, then shows what CMake adds and why the project ships both.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Dependency graph** = "a map of which files need which other files to exist and be up to date" — Make reads this map and only recompiles the files that have actually changed, so editing `analysis.c` rebuilds `build/analysis.o` and relinks `cargoforge` without touching the other thirteen source files.
- **Object file** = "a compiled chunk of one source file — machine code that still has holes where it calls functions from other files" — `analysis.o` knows how to call `hydro_interpolate` but doesn't contain it yet; the linker fills that hole by combining all `.o` files into the final `cargoforge` binary.
- **Pattern rule** = "one recipe written with a wildcard that handles every matching file automatically" — the single rule `$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)` covers all fourteen source files so you never have to write a separate compile command for `parser.c`, `placement_3d.c`, or any other file added later.
- **Target-specific variable** = "a variable override that applies only when building one particular target" — `test-asan` appends `-fsanitize=address` this way, which is how the heap-use-after-free in `parse_cargo_list` was caught: the sanitizer turned a silent stale-pointer read into an immediate crash.
- **Static vs. shared library** = "static means the engine code is baked into your binary at build time; shared means it's a separate file loaded at runtime and shared across all programs that use it" — the `lib` target produces both `libcargoforge.a` (static, self-contained) and `libcargoforge.dylib`/`.so` (shared, smaller binary but requires the `.so` to be present at runtime).
- **CMake as a meta-build system** = "a tool that generates Makefiles (or Ninja files, or IDE projects) from a cleaner, platform-neutral description" — `add_library(cargoforge_static STATIC ...)` replaces the `ar rcs` command and the `$(shell uname -s)` platform-detection dance; CMake figures out the right low-level commands for the host OS automatically.
- **Phony target** = "a Make label for an action, not a file to produce" — `.PHONY: clean test` tells Make to always run `clean` or `test` when asked, even if a file called `clean` or `test` happens to exist on disk.

**Why it matters:** if the dependency graph is wrong or a source file is missing from both `LIB_SRCS` and `LIB_SOURCES`, changes to that file silently won't rebuild — exactly the kind of stale-binary mismatch that shows up at sea rather than at the desk; getting the build system right is what makes every other fix actually land in the shipped binary.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> A **ship manifest** lists every cargo container and which bay it goes into. The port crane doesn't move a container unless something on the manifest changed since the last voyage. Make is the crane operator: it reads the dependency manifest, checks timestamps, and only touches the containers (source files) that have actually changed.

When you edit `analysis.c`, the manifest says that bay (`build/analysis.o`) is now stale — so the crane recompiles exactly that one object file and then re-stows the ship (`cargoforge` binary) by linking everything together. The other thirteen bays stay put.

CMake is one level up: it's the **manifest-writer**. Instead of hand-scribing the tab-indented Makefile rules (with their platform `uname` dances and `ar rcs` incantations), you declare intent — `add_library(cargoforge_static STATIC ...)` — and CMake prints the manifest for whichever port (macOS, Linux, Windows) the crane will run in.

---

<svg viewBox="0 0 620 260" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Make build graph for CargoForge-C</title>
  <desc>A dependency graph showing how Make compiles fourteen .c source files into .o object files, then links them into the cargoforge binary and libcargoforge library. CMake generates the same Makefile from a higher-level description.</desc>

  <!-- Source files column -->
  <rect x="10" y="20" width="140" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.2"/>
  <text x="80" y="37" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.55">src/analysis.c</text>

  <rect x="10" y="56" width="140" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.2"/>
  <text x="80" y="73" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.55">src/parser.c</text>

  <rect x="10" y="92" width="140" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.2"/>
  <text x="80" y="109" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.55">src/hydrostatics.c</text>

  <rect x="10" y="128" width="140" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.2"/>
  <text x="80" y="145" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.55">src/placement_3d.c</text>

  <rect x="10" y="164" width="140" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.35" stroke-width="1.2"/>
  <text x="80" y="181" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.55">src/main.c  …  (×14)</text>

  <!-- Arrow: src → pattern rule -->
  <line x1="150" y1="110" x2="200" y2="110" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.4" marker-end="url(#arr)"/>

  <!-- Pattern rule box -->
  <rect x="200" y="88" width="148" height="44" rx="5" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.5"/>
  <text x="274" y="107" text-anchor="middle" font-size="11" font-weight="600" fill="#12A594">pattern rule</text>
  <text x="274" y="122" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.7">$(BUILD_DIR)/%.o</text>

  <!-- Arrows: pattern rule → .o files -->
  <line x1="348" y1="110" x2="398" y2="75" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.2" marker-end="url(#arr)"/>
  <line x1="348" y1="110" x2="398" y2="110" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.2" marker-end="url(#arr)"/>
  <line x1="348" y1="110" x2="398" y2="145" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.2" marker-end="url(#arr)"/>

  <!-- .o files column -->
  <rect x="398" y="60" width="108" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.45" stroke-width="1.2"/>
  <text x="452" y="77" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.7">build/analysis.o</text>

  <rect x="398" y="96" width="108" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.45" stroke-width="1.2"/>
  <text x="452" y="113" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.7">build/parser.o</text>

  <rect x="398" y="132" width="108" height="26" rx="4" fill="none" stroke="currentColor" stroke-opacity="0.45" stroke-width="1.2"/>
  <text x="452" y="149" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.7">… (×14)</text>

  <!-- Arrows: .o → outputs -->
  <line x1="506" y1="90" x2="540" y2="72" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.2" marker-end="url(#arr)"/>
  <line x1="506" y1="120" x2="540" y2="138" stroke="currentColor" stroke-opacity="0.4" stroke-width="1.2" marker-end="url(#arr)"/>

  <!-- Final outputs -->
  <rect x="540" y="52" width="72" height="30" rx="5" fill="#12A594" fill-opacity="0.15" stroke="#12A594" stroke-width="1.8"/>
  <text x="576" y="71" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">cargoforge</text>

  <rect x="540" y="122" width="72" height="30" rx="5" fill="#12A594" fill-opacity="0.15" stroke="#12A594" stroke-width="1.8"/>
  <text x="576" y="141" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">libcargoforge</text>

  <!-- include/cargoforge.h dep line -->
  <rect x="10" y="210" width="140" height="26" rx="4" fill="none" stroke="#D05663" stroke-opacity="0.6" stroke-width="1.2" stroke-dasharray="4 3"/>
  <text x="80" y="227" text-anchor="middle" font-size="11" fill="#D05663" fill-opacity="0.85">include/cargoforge.h</text>
  <line x1="150" y1="223" x2="200" y2="223" stroke="#D05663" stroke-opacity="0.5" stroke-width="1.2" stroke-dasharray="4 3"/>
  <line x1="230" y1="223" x2="230" y2="132" stroke="#D05663" stroke-opacity="0.5" stroke-width="1.2" stroke-dasharray="4 3" marker-end="url(#arrd)"/>
  <text x="162" y="218" text-anchor="middle" font-size="9" fill="#D05663" fill-opacity="0.8">change → rebuild all .o</text>

  <!-- CMake label at bottom -->
  <text x="274" y="245" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.45">CMake generates this same graph from CMakeLists.txt  →  no hand-written tabs, no uname dance</text>

  <!-- Arrowhead markers -->
  <defs>
    <marker id="arr" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L0,7 L7,3.5 Z" fill="currentColor" fill-opacity="0.45"/>
    </marker>
    <marker id="arrd" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L0,7 L7,3.5 Z" fill="#D05663" fill-opacity="0.6"/>
    </marker>
  </defs>
</svg>

## The Problem Make Solves

When you type `gcc -O3 -std=c99 -c src/analysis.c -o build/analysis.o`, you create an **object file**: compiled machine code that is not yet a runnable program. Object files cannot run alone because they reference functions defined elsewhere (`perform_analysis` calls `hydro_interpolate`, which lives in `hydrostatics.c`). The **linker** combines object files into a final executable.

Doing this manually for fourteen files is tedious, but the deeper problem is correctness. If you change [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h), every `.c` file that includes it must be recompiled. Miss one and you get silent mismatches between the compiled binary and the current source — the kind of bug that only shows up at sea.

Make solves this by tracking a **dependency graph**: a description of which files depend on which other files, and what command to run when a dependency is newer than its output.

---

## Anatomy of a Makefile Rule

Every rule in a Makefile has three parts:

```makefile
target: dependency1 dependency2 ...
	command
```

The **target** is the file to build. The **dependencies** are the files it requires. The **command** (indented with a real tab character, not spaces) runs when any dependency is newer than the target.

From [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) in the project root:

```makefile
$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(ALL_OBJS) $(LDFLAGS)
```

`$(TARGET)` expands to `cargoforge`. This rule says: to build the `cargoforge` binary, link all object files together. If any object file has changed since `cargoforge` was last built, re-run the link command.

---

## Variables and Pattern Rules

The top of the Makefile defines variables that flow through every rule:

```makefile
CC     = gcc
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS = -lm
```

`-lm` tells the linker to include the C math library, which `analysis.c` requires for `sin`, `atan`, and `sqrt`. `-Iinclude` tells the compiler where to find header files like `cargoforge.h`.

Source groups are kept separate so that the library and the CLI can be combined differently:

```makefile
LIB_SRCS = $(SRC_DIR)/parser.c $(SRC_DIR)/analysis.c $(SRC_DIR)/placement_3d.c \
           $(SRC_DIR)/constraints.c $(SRC_DIR)/json_output.c \
           $(SRC_DIR)/hydrostatics.c $(SRC_DIR)/tanks.c \
           $(SRC_DIR)/longitudinal_strength.c $(SRC_DIR)/imdg.c \
           $(SRC_DIR)/libcargoforge.c

CLI_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/cli.c $(SRC_DIR)/visualization.c \
           $(SRC_DIR)/server.c
```

`LIB_SRCS` is the core engine — no `main()`, no CLI, no server. `CLI_SRCS` adds the user-facing layer. Both groups are compiled separately so that `libcargoforge.a` can be shipped without the CLI code.

Source paths are converted to object paths using `patsubst` (pattern substitution):

```makefile
LIB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))
```

This transforms [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) → `build/parser.o`, [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) → `build/analysis.o`, and so on for every file in the list.

The rule that compiles any single source file is a **pattern rule**:

```makefile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@
```

`%` is a wildcard: this one rule handles all fourteen source files. `$<` expands to the first dependency (the `.c` file); `$@` expands to the target (the `.o` file). Notice `$(HDRS)` — all headers are listed as dependencies. If any header changes, every object file is rebuilt. That is the correct behaviour: a header change can alter struct layouts or function signatures in any translation unit.

`-fPIC` (Position-Independent Code) is required for shared library output — it tells the compiler to generate code that can be loaded at any memory address.

---

## The Build Graph

Make works by constructing a directed acyclic graph (DAG) of targets and dependencies, then walking it from the leaves upward.

For the main `cargoforge` binary, the graph looks like this:

```
cargoforge
├── build/main.o        ← src/main.c + all headers
├── build/cli.o         ← src/cli.c + all headers
├── build/visualization.o
├── build/server.o
├── build/parser.o      ← src/parser.c + all headers
├── build/analysis.o
├── build/placement_3d.o
├── build/constraints.o
├── build/json_output.o
├── build/hydrostatics.o
├── build/tanks.o
├── build/longitudinal_strength.o
├── build/imdg.o
└── build/libcargoforge.o
```

Make compares the modification timestamp of each `.o` file against its `.c` file and all headers. Only out-of-date object files are recompiled. On a machine with parallel jobs (`make -j8`), independent nodes in the graph can be compiled simultaneously.

**Incremental builds** are the payoff. After the first full compile, editing [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) recompiles only `build/analysis.o` and then relinks `cargoforge` — typically under a second rather than several seconds for a full rebuild.

---

## Phony Targets

Some Make targets do not produce a file — they are labels for actions. Without the `.PHONY` declaration, Make would check for a file called `clean` on disk and skip the rule if one existed. The Makefile declares:

```makefile
.PHONY: all lib clean install test test-asan test-valgrind fuzz wasm example validate
```

This tells Make: regardless of whether a file named `clean` exists, always run the recipe when asked.

---

## Library Targets

The `lib` target produces two forms of `libcargoforge`:

```makefile
libcargoforge.a: $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

libcargoforge.$(SHARED_EXT): $(LIB_OBJS)
	$(CC) $(SHARED_FLAGS) -o $@ $(LIB_OBJS) $(LDFLAGS)
```

`ar rcs` is the **archiver**: it bundles object files into a static library (`.a`). A program linked against a static library carries a private copy of the engine inside its own binary. The shared library (`.dylib` on macOS, `.so` on Linux) is loaded at runtime and shared across all programs that use it.

The shared library extension is detected at the top of the Makefile:

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

`$(shell ...)` executes a shell command at Makefile parse time and substitutes the result. This is how Make handles platform differences without requiring the developer to edit the file.

---

## Sanitizer and Fuzzer Targets

The `test-asan` target rewrites `CFLAGS` and `LDFLAGS` before delegating to `all` and `test`:

```makefile
test-asan: CFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
test-asan: LDFLAGS += -fsanitize=address -fsanitize=undefined
test-asan: clean all test
	@echo "=== Memory safety tests (ASAN/UBSAN) passed ==="
```

The `+=` operator appends to the existing variable rather than replacing it. This is a **target-specific variable**: the new value applies only when building via the `test-asan` target. The heap-use-after-free bug described in the digest (Section 7) was found precisely via this target — the sanitizer turned a stale pointer read into an immediate `SIGABRT`.

`fuzz` is simpler: it delegates entirely to a shell script:

```makefile
fuzz:
	@./scripts/fuzz.sh
```

The `@` prefix suppresses echoing the command itself to the terminal — only the script's own output appears.

---

## What CMake Adds

Make is powerful but fragile: tabs vs. spaces cause silent failures; paths are written by hand; platform detection is shell scripting embedded in Makefile syntax. CMake is a **meta-build system** — it generates Makefiles (or Ninja files, or Xcode projects) from a higher-level description. The generated files are placed in a separate build directory, keeping the source tree clean.

[`CMakeLists.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/CMakeLists.txt) in the project root starts with:

```cmake
cmake_minimum_required(VERSION 3.10)
project(CargoForge-C VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
```

The same source groups are declared with `set(...)`:

```cmake
set(LIB_SOURCES
    src/parser.c
    src/analysis.c
    src/placement_3d.c
    src/constraints.c
    src/json_output.c
    src/hydrostatics.c
    src/tanks.c
    src/longitudinal_strength.c
    src/imdg.c
    src/libcargoforge.c
)
```

Library targets are declared by intent, not by command:

```cmake
add_library(cargoforge_static STATIC ${LIB_SOURCES})
set_target_properties(cargoforge_static PROPERTIES
    OUTPUT_NAME cargoforge
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(cargoforge_static m)

add_library(cargoforge_shared SHARED ${LIB_SOURCES})
set_target_properties(cargoforge_shared PROPERTIES
    OUTPUT_NAME cargoforge
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(cargoforge_shared m)
```

`add_library(... STATIC ...)` and `add_library(... SHARED ...)` replace the `ar rcs` and `-dynamiclib`/`-shared` commands from the Makefile. CMake handles the platform difference automatically — it knows whether the host is macOS or Linux without the `$(shell uname -s)` dance.

`POSITION_INDEPENDENT_CODE ON` is the CMake equivalent of `-fPIC`. `SOVERSION` sets the major version number embedded in the shared library filename — important for package managers that install multiple versions side by side.

### Testing with CTest

CMake comes with CTest, a test runner. Tests are registered with `add_test`:

```cmake
add_executable(test_analysis tests/test_analysis.c src/analysis.c src/hydrostatics.c src/tanks.c src/longitudinal_strength.c)
target_link_libraries(test_analysis m)
add_test(NAME test_analysis COMMAND test_analysis)
```

After building, `ctest` runs all registered tests and reports pass/fail counts. The Makefile's `test` target does the same thing but requires each binary to be listed twice — once to build it and once to run it.

### Sanitizer Options

The CMake file exposes sanitizers as build options rather than separate targets:

```cmake
option(BUILD_WITH_ASAN "Build with AddressSanitizer" OFF)
option(BUILD_WITH_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)

if(BUILD_WITH_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()
```

You enable them at configuration time:

```bash
cmake -DBUILD_WITH_ASAN=ON -DBUILD_WITH_UBSAN=ON -B build-asan
cmake --build build-asan
```

This approach produces a separate build directory for the sanitized binary, leaving the release build untouched — whereas `make test-asan` runs `clean` first and destroys the existing build.

---

## Using the Build System

```bash
# Full build (Makefile)
make

# Incremental rebuild after editing src/analysis.c
make          # only analysis.o and cargoforge are regenerated

# Library only
make lib

# Run tests
make test

# Memory safety pass
make test-asan

# Install to /usr/local
make install

# CMake workflow (out-of-tree)
cmake -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake
```

!!! note
    Always use an **out-of-tree build** with CMake (`-B build-cmake`). This keeps generated files out of the source tree and makes `git status` clean.

!!! tip
    When you add a new source file, update both `LIB_SRCS` in the Makefile **and** `LIB_SOURCES` in [`CMakeLists.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/CMakeLists.txt). The project ships both build systems; keeping them in sync is part of adding a file.

---

## Why Two Build Systems?

The Makefile is the primary, self-contained build for contributors: no external tools needed, familiar to anyone who has compiled C before, and it directly controls the WASM and fuzzer targets that CMake does not replicate. CMake is provided for integrators — IDEs, package managers, and CI pipelines that prefer a platform-neutral description they can query and extend. The `lib` target in the Makefile and `add_library` in CMake both produce `libcargoforge.a` and the shared library; the public API surface (`libcargoforge.h`) is the same either way.

---

## Recap

- A **rule** maps a target file to its dependencies and the command to rebuild it when any dependency is newer.
- The **pattern rule** `$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)` compiles all fourteen source files through one recipe; all headers are listed as dependencies so a header change triggers a full rebuild.
- `patsubst` converts source paths to object paths mechanically; `$(shell ...)` runs shell commands at parse time for platform detection.
- `libcargoforge.a` is built with `ar rcs`; the shared library uses `-dynamiclib` (macOS) or `-shared` (Linux), selected by `uname` at Makefile parse time.
- `test-asan` appends `-fsanitize=address,undefined` via target-specific variables; it was this target that caught the heap-use-after-free in `parse_cargo_list`.
- CMake generates Makefiles from a higher-level description: `add_library`, `add_executable`, and `add_test` replace hand-written recipes and remove platform-specific shell scripting.

*Next: [Unit testing in C](15-unit-testing-in-c.md).*
