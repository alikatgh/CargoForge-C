# Static analysis

Static analysis means examining source code for bugs *before* compiling or running it. For CargoForge-C this matters because the program handles heap-allocated cargo arrays, raw pointer arithmetic over parsed manifests, and numeric parsing on untrusted input — exactly the class of code where subtle errors survive every unit test yet crash under an adversarial manifest.

---

## What static analysis is — and is not

When you run a test suite, the program executes a finite set of inputs. Static analysis works differently: it reasons over all possible inputs at once, inspecting the source text (or the compiler's intermediate representation) without ever running the binary. That lets it find errors that only manifest on unusual inputs, or on a code path your tests never reach.

The three main forms used in this project:

| Tool | When it runs | What it sees |
|------|-------------|--------------|
| Compiler warnings (`-Wall -Wextra`) | Every build | Syntax + data-flow in each translation unit |
| Clang Static Analyzer / `cppcheck` | On-demand | Deeper cross-statement data-flow and memory paths |
| Sanitizers (`-fsanitize=…`) | Test builds | *Dynamic* — runs, but catches whole classes of UB |

Sanitizers are covered in the next lesson. This lesson focuses on the first two: analysis that happens before any execution.

---

## The compiler as the first static analyzer

CargoForge-C compiles with these flags, as written in the [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile):

```makefile
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
```

`-Wall` and `-Wextra` are not the same flag: `-Wall` enables a well-curated set of warnings that almost always indicate real bugs; `-Wextra` adds a second tier that is occasionally noisy but catches real problems in C code. Together they warn about:

| Warning class | Example trigger |
|---------------|----------------|
| Implicit fall-through in `switch` | Missing `break` before next `case` |
| Unused variables / parameters | A local declared but never read |
| Signed/unsigned comparison | `int i < strlen(s)` |
| Missing return value | Non-`void` function that sometimes falls off the end |
| Comparing floats with `==` | Direct equality on `float` or `double` |
| Shadowed variables | Inner `int i` hiding an outer `int i` |
| Uninitialized reads | Variable used before assignment on some path |

These are not stylistic preferences. Each warning class has caused real production bugs in shipping software. Leaving them enabled means a silent regression shows up as a warning on the very next build, before any test is even run.

!!! tip
    Build with `-Werror` in CI to make warnings fail the build. CargoForge-C does not do this yet, but the convention is common in safety-critical C projects because it prevents warning debt from accumulating.

---

## What the compiler catches in `parser.c`

[`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) provides a good case study. The parser handles untrusted text from two sources — ship config files and cargo manifests — so every numeric field goes through `safe_atof`:

```c
static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    errno = 0;
    float val = strtof(s, &end);

    if (errno != 0 || end == s || (*end != '\0' && *end != '\n') || val < min || val > max) {
        fprintf(stderr, "Error: Invalid or out-of-range %s value '%s'\n", field_name, s);
        return NAN;
    }
    return val;
}
```

Notice the condition `end == s`: this catches the case where `strtof` consumed zero characters — meaning the string was not a number at all. And `*end != '\0' && *end != '\n'` rejects trailing garbage like `"12.5abc"`. None of these conditions can be checked by looking at the return value of `atof` alone; `atof` silently returns 0 for junk input. The compiler cannot warn about the design choice, but it *will* warn if you try `(void)strtof(s, NULL)` and discard `end` — `-Wunused-result` fires on functions whose return value carries error information.

The call sites in `parse_ship_config` (line 86) and `parse_cargo_list` (line 332) pass the result straight into an `isnan` check:

```c
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;   // avoid a dangling pointer -> use-after-free/double-free in ship_cleanup
    ship->cargo_count = 0;
    ...
    return -1;
}
```

The comment is instructive: this NULL-assignment was added to fix a real heap-use-after-free (described in the digest, section 7). The compiler does not catch use-after-free — that requires a sanitizer or a static analyzer with interprocedural reach. But the compiler *would* warn if `ship->cargo` were read without a preceding NULL check, because `-Wmaybe-uninitialized` tracks uninitialized paths.

---

## Going deeper: Clang Static Analyzer and `cppcheck`

`-Wall -Wextra` are intraprocedural: they reason within a single function at a time. They cannot see that `parse_cargo_list` frees `ship->cargo` on an error path and that `ship_cleanup` later reads it. For that you need an interprocedural analyzer.

### Clang Static Analyzer

Invoked as a drop-in replacement for the compiler:

```bash
scan-build make
```

This re-runs every compilation step under the analyzer's wrapper and produces an HTML report of discovered defects. Relevant checker families for a codebase like CargoForge-C:

| Checker | What it tracks |
|---------|---------------|
| `unix.Malloc` | Allocated memory not freed on all paths; double-free |
| `core.NullDereference` | Pointer used after a NULL-returning call |
| `alpha.security.ArrayBound` | Array index exceeding its declared size |
| `unix.API` | Misuse of POSIX calls (e.g. calling `strlen` on a non-null-terminated buffer) |

The analyzer would flag the original (unfixed) version of `parse_cargo_list`: it can trace the fact that `ship->cargo` is freed on the error path, `ship->cargo_count` is left non-zero, and `ship_cleanup` then iterates up to `ship->cargo_count` accessing the freed pointer. That is a multi-function path spanning two translation units — invisible to `-Wall`.

### `cppcheck`

`cppcheck` is a standalone open-source tool (not LLVM-based) that excels at C99/C11 and has a lower false-positive rate for buffer-overflow checks than the Clang alpha checkers:

```bash
cppcheck --enable=all --std=c99 src/
```

Particularly relevant for CargoForge-C:

- `bufferAccessOutOfBounds`: catches `strncpy(buf, field + 3, sizeof(buf) - 1)` in `parse_dg_field` (line 160 of `parser.c`) — the analyzer verifies that `sizeof(buf) - 1 = 63` cannot exceed the declared `char buf[64]`.
- `memleak`: traces `calloc` returns that are not freed on all paths.
- `uninitvar`: similar to `-Wmaybe-uninitialized` but interprocedural.

---

## What static analysis cannot catch

Static analyzers are conservative: they report what they can prove, not everything that is wrong. Their blind spots are real:

- **Logic errors**: if `gz_at_angle` implements the wrong formula, no tool will flag it. The formula must be verified against a naval-architecture reference.
- **Race conditions**: CargoForge-C's server (`server.c`) is single-threaded, so this is not currently an issue, but a concurrent version would need a thread-safety analyzer (`helgrind`, `tsan`).
- **Semantic range errors**: `safe_atof` enforces `[0.1, 1e9]` for numeric fields. Whether that range is physically meaningful for a given field (e.g. whether 1 000 km is a sensible ship length) is domain knowledge the analyzer does not have.
- **Integration failures**: the parser may produce a well-formed `Ship` with contradictory values — a box-hull ship with a hydrostatic table path that points to the wrong vessel. Tests and validation cover this; static analysis does not.

This is why CargoForge-C combines multiple layers: `-Wall -Wextra` on every build, sanitizers in `make test-asan`, Valgrind in `make test-valgrind`, and a purpose-built fuzzer in [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh). Static analysis is the cheapest layer — zero runtime cost — so it should always be on.

---

## How the sanitizer layer complements static analysis

The Makefile's `test-asan` target adds flags to both compile and link steps:

```makefile
test-asan: CFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
test-asan: LDFLAGS += -fsanitize=address -fsanitize=undefined
test-asan: clean all test
```

AddressSanitizer (ASan) is what actually *caught* the heap-use-after-free in `parse_cargo_list` during development — the static analyzer gives you a warning at compile time; ASan gives you a precise stack trace pointing to the exact freed address at runtime. The two tools are complementary: the static analyzer finds the reachable-but-untested path; the sanitizer confirms whether it is truly a bug and tells you exactly where.

!!! note
    `-fno-omit-frame-pointer` keeps stack frames intact so that ASan can print a full call chain. Without it, the report may truncate, making the bug harder to locate.

---

## Recap

- `-Wall -Wextra` are intraprocedural: they reason within one function at compile time and catch uninitialized reads, type mismatches, and unused values — zero runtime cost.
- `scan-build` (Clang Static Analyzer) and `cppcheck` add interprocedural analysis, tracing pointer lifetimes and NULL paths across function call boundaries.
- The heap-use-after-free fixed in `parse_cargo_list` (setting `ship->cargo = NULL` and `ship->cargo_count = 0` after `free`) is exactly the class of multi-function bug that interprocedural analysis catches and unit tests routinely miss.
- Static analysis does not verify domain correctness (wrong physics formula, wrong range for a numeric field) — that requires tests and validation against known benchmarks.
- In CargoForge-C, static analysis is the *first* defense layer; sanitizers (`make test-asan`) and Valgrind (`make test-valgrind`) run on top of it for dynamic confirmation.

*Next: [Fuzzing](40-fuzzing.md).*
