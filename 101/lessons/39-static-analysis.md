# Static analysis

Static analysis means examining source code for bugs *before* compiling or running it. For CargoForge-C this matters because the program handles heap-allocated cargo arrays, raw pointer arithmetic over parsed manifests, and numeric parsing on untrusted input — exactly the class of code where subtle errors survive every unit test yet crash under an adversarial manifest.

## The mental model 🧠

Static analysis is proofreading the code without ever running it. Where a sanitizer must *execute* a bad path to catch it, a static analyser *reads* the code and reasons about every path at once — "if this branch is taken, `p` is NULL here, and the next line dereferences it." It finds bugs your tests never triggered, because it does not need an input that triggers them; it needs only that the dangerous path *exists*.

The reach matters. Plain compiler warnings (`-Wall -Wextra`) look *inside* one function at a time, so they missed the `parse_cargo_list` use-after-free — that bug spanned two functions (freed in one, read in `ship_cleanup`). An *interprocedural* tool like `scan-build` follows calls across the whole program and can trace exactly that path. The trade-off is the mirror of sanitizers: static analysis sees *all* paths but not real values, so it raises false positives; sanitizers see *real* runs with none, but only the paths your tests drove. And neither can tell you your physics is wrong — a bad `gz_at_angle` passes every linter. Read the code *and* run the code.

<svg viewBox="0 0 600 210" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Static analysis reads every path through the code without running it</title>
<desc>A static analyser examines the source for all possible execution paths and flags dangerous ones — a possible NULL dereference, a leak on an error path — without ever executing the program. It complements runtime sanitizers, which catch only the paths the tests actually drive.</desc>
<text x="184" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">source code — examined, not executed</text>
<rect x="24" y="34" width="320" height="116" rx="5" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.35"/>
<g font-family="var(--md-code-font,monospace)" font-size="10.5" fill="currentColor" opacity="0.8">
<text x="38" y="56">Space &#42;p = find_space(bin);</text>
<text x="38" y="78">if (cond)</text>
<text x="38" y="100">    p = NULL;</text>
<rect x="30" y="108" width="308" height="20" fill="#D05663" fill-opacity="0.1"/>
<text x="38" y="123" fill="currentColor">p-&gt;x = 0;</text>
</g>
<text x="250" y="123" font-size="9" fill="#D05663" opacity="0.9">⚠ may be NULL</text>
<text x="38" y="144" font-size="8.5" fill="currentColor" opacity="0.5" font-family="var(--md-code-font,monospace)">// no input needed — the path just has to exist</text>
<line x1="344" y1="92" x2="392" y2="92" stroke="#12A594" stroke-opacity="0.6"/><path d="M385,88 L392,92 L385,96" fill="none" stroke="#12A594"/>
<rect x="394" y="44" width="182" height="96" rx="5" fill="#12A594" fill-opacity="0.06" stroke="#12A594" stroke-opacity="0.45"/>
<text x="406" y="64" font-size="9.5" fill="currentColor" opacity="0.8">flagged before runtime:</text>
<text x="406" y="84" font-size="9" fill="currentColor" opacity="0.75">⚠ possible NULL deref</text>
<text x="406" y="102" font-size="9" fill="currentColor" opacity="0.75">⚠ leak on error path</text>
<text x="406" y="120" font-size="9" fill="currentColor" opacity="0.75">⚠ unused result</text>
<text x="300" y="190" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.7">static: all paths (may false-positive) · sanitizers: real runs (no false-positive)</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Static analysis** = "reading your code for bugs without running it" — the compiler and tools like `scan-build` or `cppcheck` reason over every possible input at once, which is how they catch the bug in `parse_cargo_list` that your test suite never triggered.
- **Intraprocedural vs. interprocedural** = "one function at a time vs. following calls across the whole program" — `-Wall -Wextra` only see inside a single function, so they miss the use-after-free where `parse_cargo_list` frees `ship->cargo` and `ship_cleanup` reads it later; an interprocedural tool like `scan-build` can trace that multi-function path.
- **`safe_atof`** = "a wrapper around `strtof` that actually tells you when the input was garbage" — unlike `atof`, it checks `end == s` (nothing was parsed) and trailing characters like `"12.5abc"`, returning `NaN` so callers in `parse_cargo_list` can bail out cleanly instead of silently using a zero.
- **Use-after-free** = "reading memory you already handed back to the heap" — the real bug fixed in `parse_cargo_list` was that `ship->cargo` was freed on an error path but `ship->cargo_count` stayed non-zero, so `ship_cleanup` later iterated over freed slots; the fix was setting `ship->cargo = NULL` and `ship->cargo_count = 0` immediately after `free`.
- **`-Werror` in CI** = "turn every compiler warning into a build failure" — CargoForge-C doesn't use this yet, but it's the mechanism that prevents warning debt from quietly piling up across commits.
- **Static analysis blind spots** = "tools can't know if your physics formula is wrong" — `gz_at_angle` could implement the wrong righting-lever calculation and pass every static check; domain correctness (right range, right formula) requires tests and benchmarks, not a linter.

**Why it matters:** a use-after-free in `parse_cargo_list` survived unit tests and only surfaced under an adversarial manifest — exactly the scenario static analysis and sanitizers exist to catch before the ship's stability report is trusted with real cargo.

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

The comment is instructive: this NULL-assignment was added to fix a real heap-use-after-free (described in [Lesson 13](13-memory-bugs.md)). The compiler does not catch use-after-free — that requires a sanitizer or a static analyzer with interprocedural reach. But the compiler *would* warn if `ship->cargo` were read without a preceding NULL check, because `-Wmaybe-uninitialized` tracks uninitialized paths.

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

## Check yourself

??? question "Why did plain compiler warnings (-Wall -Wextra) miss the real use-after-free bug, when an interprocedural tool like scan-build could catch it?"
    `-Wall`/`-Wextra` only reason about one function at a time. The bug spanned two functions — freed in `parse_cargo_list`, read in `ship_cleanup` — which needs a tool that follows calls *across* functions to see the whole dangerous path.

??? question "What's something static analysis fundamentally cannot catch, no matter how good the tool?"
    Whether the domain logic is *correct*. A wrong physics formula — say, a sign error in `gz_at_angle` — can pass every static check perfectly while still computing the wrong answer. Catching that needs tests and benchmarks against known-good values, not a linter.

*Next: [Fuzzing](40-fuzzing.md).*
