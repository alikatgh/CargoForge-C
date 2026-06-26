# Lab 10 — The Quality Gate

Every professional C project has a quality gate: a set of automated checks that must all
pass before code is trusted. CargoForge-C's gate has four layers — unit tests, memory-safety
tests with sanitizers, a random-input fuzzer, and a code-coverage build. This lab walks you
through each layer in order, shows you what passing and failing output looks like, and explains
why the program is designed to never crash even on garbage input.

---

## 0. Prerequisites

A working build and the repository root as your working directory.

```bash
make
```

The binary `./cargoforge` must exist. If it does not, revisit Lab 4 (the Makefile).

---

## 1. The Unit Test Suite — `make test`

### What the suite covers

The Makefile builds and runs eight test binaries. Each one is linked against only the
object files it needs, so a failure pinpoints the broken module immediately.

| Binary | Module(s) exercised |
|---|---|
| `tests/test_parser` | `parser.c`, `hydrostatics.c`, `tanks.c`, `longitudinal_strength.c` |
| `tests/test_analysis` | `analysis.c` and the hydro/tank/strength modules it calls |
| `tests/test_constraints` | `constraints.c`, `placement_3d.c`, `imdg.c` |
| `tests/test_hydrostatics` | `hydrostatics.c` — table parsing and interpolation |
| `tests/test_tanks` | `tanks.c` — free-surface moment calculations |
| `tests/test_longitudinal_strength` | `longitudinal_strength.c` — SWSF/SWBM |
| `tests/test_imdg` | `imdg.c` — segregation matrix lookups |
| `tests/test_library` | Full public API via `libcargoforge.a` |

### Step 1 — Run the suite

From the repository root:

```bash
make test
```

### Expected output

```
--- Running All Tests ---
All parser tests passed.
All analysis tests passed.
All constraint tests passed.
All hydrostatics tests passed.
All tank tests passed.
All longitudinal strength tests passed.
All IMDG tests passed.
All library tests passed.
-----------------------
```

Every binary exits with code 0. The `-----------------------` line is the Makefile's final
echo confirming all eight ran without a non-zero exit.

!!! note "What does a failure look like?"
    If a test binary exits non-zero, `make` stops immediately and prints something like:
    ```
    tests/test_analysis: FAIL — gm_corrected mismatch (expected 1.23, got 0.00)
    make: *** [test] Error 1
    ```
    The binary name tells you which module broke. Run it directly
    (`./tests/test_analysis`) to see the full error message without `make`'s noise.

---

## 2. Memory-Safety Tests — `make test-asan`

The unit tests above check *correctness*. AddressSanitizer (ASan) and UndefinedBehavior
Sanitizer (UBSan) check *memory safety* — heap overflows, use-after-free, null dereferences,
and signed integer overflow — things that can corrupt data silently or crash in production
without ever triggering a logic assertion.

### How it works

`make test-asan` does three things in sequence:

1. **`clean`** — removes all previously compiled objects.
2. **`all`** — recompiles everything with extra flags:
   ```
   -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
   ```
   Both the compile step *and* the link step carry these flags so the sanitizer
   runtime is embedded in every binary.
3. **`test`** — runs the full test suite. If any binary triggers a sanitizer error,
   the process is aborted immediately (exit ≥ 128).

### Step 2 — Run the sanitizer build

```bash
make test-asan
```

This takes longer than `make test` because it recompiles everything from scratch with
`-O1 -g` effective optimization (ASan is incompatible with `-O3`'s most aggressive
inlining).

### Expected output

```
--- Running All Tests ---
All parser tests passed.
All analysis tests passed.
All constraint tests passed.
All hydrostatics tests passed.
All tank tests passed.
All longitudinal strength tests passed.
All IMDG tests passed.
All library tests passed.
-----------------------
=== Memory safety tests (ASAN/UBSAN) passed ===
```

The final line is the Makefile's confirmation that sanitizer-instrumented execution
completed cleanly.

!!! warning "What a sanitizer failure looks like"
    A real failure — the kind this codebase actually fixed — looks like:
    ```
    =================================================================
    ==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
        #0 0x... in ship_cleanup src/analysis.c:212
        #1 0x... in main src/main.c:31
    ```
    The report names the exact file and line. This is the error that was produced
    when `parse_cargo_list` freed `ship->cargo` on a parse error but left
    `ship->cargo_count` non-zero. `ship_cleanup` then walked the freed array.
    The fix was two lines in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c):
    ```c
    ship->cargo = NULL;
    ship->cargo_count = 0;
    ```
    A clean error exit (the parser *rejects* bad input) is a pass. A crash is a
    fail. That distinction is the contract.

---

## 3. The Fuzzer — [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)

Unit tests check the inputs *you thought of*. The fuzzer checks inputs you did not think
of — malformed configs, numbers out of range, truncated dimension strings, binary garbage,
adversarial DG field variants.

### How the fuzzer works

[`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) builds a separate sanitized binary (`build/cargoforge-asan`) if it does
not already exist, then runs 300 iterations (by default). Each iteration:

1. Generates a random ship config by mixing keys from a known set with adversarial values
   (`-5`, `0`, `abc`, `1e9`, `/nonexistent.csv`, empty string, …) and random comment /
   malformed-key patterns.
2. Generates a random cargo manifest with too-few fields, binary garbage lines, valid DG
   strings, and intentionally broken DG strings (`DG:::`, `DG:abc`).
3. Calls `build/cargoforge-asan optimize` or `build/cargoforge-asan validate` (2:1 ratio)
   on the generated files.
4. **FAIL** if the exit code is ≥ 128 (signal kill = crash) or stderr contains
   `AddressSanitizer`, `runtime error:`, or `UndefinedBehavior`.
5. **PASS** for any clean exit code, including a non-zero rejection of bad input.

The fuzzer uses a fixed seed (`RANDOM=1`) by default so CI runs are deterministic and
reproducible — running it twice gives the same sequence of inputs.

### Step 3 — Run the fuzzer

```bash
scripts/fuzz.sh
```

The default is 300 iterations. You can override both parameters:

```bash
scripts/fuzz.sh 50          # 50 iterations, seed 1
scripts/fuzz.sh 50 42       # 50 iterations, seed 42
```

### Expected output

```
Building sanitized binary (build/cargoforge-asan)...
Fuzz OK: 300 iterations, no crashes or sanitizer errors.
```

If the sanitized binary already exists from a previous run, the "Building…" line is skipped.
The fuzzer exits with code 0.

!!! tip "What the fuzzer cannot catch"
    The fuzzer is a *robustness* tool: it verifies that the program never crashes on bad
    input. It does not verify correctness on valid input — that is the unit tests' job.
    For a correctness guarantee on a specific edge case, write a test in
    [`tests/test_parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/tests/test_parser.c) or the relevant test file and add it to `make test`.

---

## 4. A Coverage Build — Seeing What the Tests Actually Touch

Coverage measurement answers a specific question: which lines of source code were actually
executed when the tests ran? A line that was never reached is either dead code or an
untested code path — a candidate for the next test to write.

The Makefile does not have a built-in `make coverage` target, so you build it manually
with `gcov` flags. GCC and Clang both support this.

### Step 4 — Build with coverage instrumentation

```bash
mkdir -p build/cov

gcc -O0 -g --coverage -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
    src/parser.c src/analysis.c src/placement_3d.c src/constraints.c \
    src/json_output.c src/hydrostatics.c src/tanks.c \
    src/longitudinal_strength.c src/imdg.c src/libcargoforge.c \
    src/main.c src/cli.c src/visualization.c src/server.c \
    -lm -o build/cov/cargoforge-cov
```

`--coverage` compiles in two extra things: `.gcno` note files (the instrumentation map)
and, at runtime, `.gcda` data files that accumulate hit counts per line.

### Step 5 — Generate coverage data by running the test inputs

```bash
cd build/cov

./cargoforge-cov optimize ../../examples/sample_ship.cfg ../../examples/sample_cargo.txt
./cargoforge-cov optimize ../../examples/sample_ship.cfg ../../examples/sample_cargo_dg.txt
./cargoforge-cov validate ../../examples/bad_ship.cfg ../../examples/sample_cargo.txt
./cargoforge-cov info     ../../examples/sample_ship.cfg ../../examples/sample_cargo.txt
./cargoforge-cov optimize --format=json \
    ../../examples/sample_ship.cfg ../../examples/sample_cargo.txt
```

Each run appends to the `.gcda` files. Running multiple scenarios accumulates more
coverage.

### Step 6 — Produce the text report

```bash
gcov -r ../../src/analysis.c
```

### Expected output (excerpt)

```
File '../../src/analysis.c'
Lines executed:87.32% of 142
Creating 'analysis.c.gcov'
```

Open `analysis.c.gcov` in any text editor. Lines that were executed show a hit count on
the left; lines that were never reached show `#####`:

```
    12:   89:    result.placed_item_count = 0;
    13:   89:    result.total_cargo_weight_kg = 0;
#####:  200:    if (ship->hydro && ship->hydro->loaded) {
```

A `#####` on the hydrostatic-table branch tells you the test inputs all used the
box-hull fallback. To cover that branch, add a run that uses
[`examples/sample_ship_full.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship_full.cfg) (which references [`examples/sample_hydro.csv`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_hydro.csv)):

```bash
./cargoforge-cov optimize ../../examples/sample_ship_full.cfg \
    ../../examples/sample_cargo.txt
gcov -r ../../src/analysis.c
```

Re-running `gcov` after this should show a higher percentage for `analysis.c`.

!!! note "Coverage is a floor, not a ceiling"
    100 % line coverage does not mean the code is correct — it means every line ran
    at least once. A line can run and still produce a wrong answer. Coverage is most
    useful for finding *untested* paths, not for proving *tested* paths are correct.

---

## 5. Putting It All Together — The Four-Layer Gate

| Layer | Command | What it catches |
|---|---|---|
| Unit tests | `make test` | Logic errors on known-good inputs |
| Sanitizer tests | `make test-asan` | Memory bugs: use-after-free, overflow, UB |
| Fuzzer | [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) | Crashes on adversarial / malformed input |
| Coverage | manual `gcov` build | Untested code paths |

The correct order is the one in this lab: unit tests first (fast, catch regressions),
then sanitizers (catch the memory bugs unit assertions miss), then the fuzzer (stress the
parser and CLI with adversarial input), then coverage (audit what was not exercised).

A branch is ready to merge when all three automated layers pass (`make test`, `make test-asan`,
[`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) and coverage does not reveal a known-important path that is completely
unreached.

---

## Solution

### Expected final state after completing all steps

**Step 1 (`make test`):**

```
--- Running All Tests ---
All parser tests passed.
All analysis tests passed.
All constraint tests passed.
All hydrostatics tests passed.
All tank tests passed.
All longitudinal strength tests passed.
All IMDG tests passed.
All library tests passed.
-----------------------
```
Exit code 0.

**Step 2 (`make test-asan`):**

```
--- Running All Tests ---
All parser tests passed.
... (same 8 lines) ...
-----------------------
=== Memory safety tests (ASAN/UBSAN) passed ===
```
Exit code 0. No `AddressSanitizer:` or `runtime error:` lines on stderr.

**Step 3 ([`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)):**

```
Fuzz OK: 300 iterations, no crashes or sanitizer errors.
```
Exit code 0.

**Steps 4–6 (coverage):**

Running `gcov -r ../../src/analysis.c` on the five sample runs produces output like:

```
Lines executed:87.32% of 142
```

After adding the `sample_ship_full.cfg` run (which loads the hydrostatic CSV table),
the percentage rises because the `hydro->loaded` branch in `perform_analysis` is now
exercised. The exact numbers vary by compiler version.

### Common pitfalls

| Symptom | Likely cause | Fix |
|---|---|---|
| `make test-asan` fails on `clean` | Object files from a non-ASan build conflict | Always start `test-asan` from a clean state — it calls `clean` automatically |
| [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) exits 127 | Script not executable | `chmod +x scripts/fuzz.sh` |
| `gcov` reports 0 % | `.gcda` files not created | Make sure you ran the coverage binary *before* calling `gcov` |
| `make test` fails: `undefined reference to sqrt` | Missing `-lm` flag | The Makefile handles this; if building manually, add `-lm` |
| Fuzzer prints `FUZZ FAIL` | A real crash or sanitizer hit | Read the printed config/manifest and stderr; check `docs/BUG_JOURNAL.md` for known patterns |

---

## Recap

- **`make test`** builds and runs eight module-scoped test binaries; a non-zero exit
  tells you exactly which module broke.
- **`make test-asan`** recompiles everything with AddressSanitizer and UBSan, then
  re-runs the suite; it caught the heap-use-after-free in `parse_cargo_list` that
  silent logic assertions could not see.
- **[`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)** throws 300 adversarial inputs at the parser and CLI; the pass
  condition is "never crash", not "never reject".
- A **coverage build** (`--coverage` + `gcov`) shows which lines were executed; `#####`
  marks untested paths worth investigating.
- The correct quality-gate order is: unit tests → sanitizers → fuzzer → coverage.

*Next: [The CLI and output formats](42-the-cli-and-output-formats.md).*
