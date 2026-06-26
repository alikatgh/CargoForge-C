# Static analysis

Static analysis means examining source code for bugs *before* compiling or running it. For CargoForge-C this matters because the program handles heap-allocated cargo arrays, raw pointer arithmetic over parsed manifests, and numeric parsing on untrusted input — exactly the class of code where subtle errors survive every unit test yet crash under an adversarial manifest.

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

## The mental model 🧠

You'll forget the flag names — hold THIS picture instead:

> Imagine a building inspector who reads the blueprints before a single brick is laid. She doesn't need to watch the house fall down to know that a load-bearing wall is missing — she can see it in the drawing. Your test suite is the tenant who discovers the leak when it rains; the static analyzer is the inspector who catches the design flaw before the roof goes on.

In CargoForge-C, the "blueprints" are the C source files. The `-Wall -Wextra` inspector works room by room (one function at a time — intraprocedural), spotting an uninitialized `float` or a signed/unsigned mismatch inside `safe_atof` without ever running the parser. `scan-build` and `cppcheck` are a senior inspector who reads the whole floor plan at once (interprocedural): they can see that `parse_cargo_list` frees `ship->cargo` on an error path and that `ship_cleanup` later walks those freed slots — a defect that spans two functions and that no single-room inspection can spot. The test suite (ASan, Valgrind) is still the tenant — essential for confirming the bug is real — but static analysis finds it before the house is built.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Static analysis layers in CargoForge-C</title>
  <desc>A flow diagram showing C source files passing through three analysis layers: compiler warnings (intraprocedural), Clang Static Analyzer / cppcheck (interprocedural), and sanitizers (dynamic). Each layer catches progressively deeper bugs, ending at the fixed parse_cargo_list use-after-free.</desc>

  <!-- Source file box -->
  <rect x="20" y="110" width="130" height="90" rx="6" fill="none" stroke="currentColor" stroke-width="1.5"/>
  <text x="85" y="133" text-anchor="middle" font-size="12" font-weight="600" fill="currentColor">C source</text>
  <text x="85" y="151" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.75">parser.c</text>
  <text x="85" y="167" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.75">ship.c</text>
  <text x="85" y="183" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.75">server.c</text>

  <!-- Arrow 1 -->
  <line x1="150" y1="155" x2="178" y2="155" stroke="currentColor" stroke-width="1.5" marker-end="url(#arr)"/>

  <!-- Layer 1: compiler warnings -->
  <rect x="178" y="90" width="130" height="130" rx="6" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="243" y="113" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">-Wall -Wextra</text>
  <text x="243" y="131" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">intraprocedural</text>
  <line x1="195" y1="141" x2="308" y2="141" stroke="currentColor" stroke-width="0.8" opacity="0.4"/>
  <text x="243" y="156" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">uninit reads</text>
  <text x="243" y="171" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">sign mismatch</text>
  <text x="243" y="186" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">unused vars</text>
  <text x="243" y="208" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.55">every build, zero cost</text>

  <!-- Arrow 2 -->
  <line x1="308" y1="155" x2="336" y2="155" stroke="currentColor" stroke-width="1.5" marker-end="url(#arr)"/>

  <!-- Layer 2: interprocedural -->
  <rect x="336" y="70" width="140" height="170" rx="6" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="406" y="93" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">scan-build</text>
  <text x="406" y="109" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">/ cppcheck</text>
  <text x="406" y="127" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">interprocedural</text>
  <line x1="350" y1="136" x2="462" y2="136" stroke="currentColor" stroke-width="0.8" opacity="0.4"/>
  <text x="406" y="152" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">cross-fn pointer</text>
  <text x="406" y="167" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">lifetime tracing</text>
  <text x="406" y="184" text-anchor="middle" font-size="10" fill="#D05663">use-after-free path</text>
  <text x="406" y="199" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">NULL deref</text>
  <text x="406" y="222" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.55">on-demand, no run</text>

  <!-- Arrow 3 -->
  <line x1="476" y1="155" x2="504" y2="155" stroke="currentColor" stroke-width="1.5" marker-end="url(#arr)"/>

  <!-- Layer 3: sanitizers -->
  <rect x="504" y="90" width="100" height="130" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <text x="554" y="113" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">ASan</text>
  <text x="554" y="129" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">Valgrind</text>
  <text x="554" y="147" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.6">dynamic</text>
  <line x1="514" y1="155" x2="598" y2="155" stroke="currentColor" stroke-width="0.8" opacity="0.4"/>
  <text x="554" y="171" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">confirms bug</text>
  <text x="554" y="186" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">stack trace</text>
  <text x="554" y="208" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.55">next lesson</text>

  <!-- Bug callout below layer 2 -->
  <rect x="316" y="258" width="180" height="42" rx="5" fill="none" stroke="#D05663" stroke-width="1.3"/>
  <text x="406" y="275" text-anchor="middle" font-size="10" fill="#D05663" font-weight="700">parse_cargo_list bug</text>
  <text x="406" y="291" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">ship-&gt;cargo freed; cargo_count &gt; 0</text>
  <!-- Dashed line from layer 2 bottom to callout -->
  <line x1="406" y1="240" x2="406" y2="258" stroke="#D05663" stroke-width="1.2" stroke-dasharray="4 3"/>

  <defs>
    <marker id="arr" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="currentColor"/>
    </marker>
  </defs>
</svg>

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
