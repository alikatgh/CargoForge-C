# Unit testing in C

C has no built-in test framework, but it does not need one. The standard library's `assert()` macro and a carefully structured `main()` are enough to write tests that catch real bugs — including the heap-use-after-free that CargoForge-C's own fuzzer found. This lesson shows you how CargoForge-C tests its parser and hydrostatics modules, and how to run the whole suite with a single `make test`.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **`assert(condition)`** = "crash loudly if this is ever false" — it is the whole test mechanism in one macro: if `parse_ship_config` returns the wrong value, `assert` aborts with the file name and line number, and `make test` sees a non-zero exit code and stops.
- **Heap-use-after-free (UAF)** = "reading memory you already gave back" — in `parse_cargo_list`, the old error path freed `ship->cargo` but left `ship->cargo_count` intact, so `ship_cleanup` later walked the freed block; the regression test guards against this by asserting `s.cargo == NULL && s.cargo_count == 0` right after the failed parse.
- **Regression test** = "a test whose only job is to make sure a fixed bug stays fixed" — Test 3 in `test_parser.c` writes a deliberately bad cargo manifest at runtime, calls `parse_cargo_list`, and pins the two invariants (`NULL` pointer, zero count) that the original buggy code violated.
- **`ASSERT_NEAR(a, b, tol, msg)`** = "close enough counts for floating-point" — hydrostatic values like KB, BM, and displacement are stored as `float`; exact `==` comparisons fail on rounding noise, so `test_hydrostatics.c` checks that the absolute difference stays within 0.01 m (1 cm).
- **Fixture** = "a tiny helper that writes the exact input your test needs, then cleans up after itself" — both `test_parser.c` (writing `_bad_cargo_test.txt` to `build/`) and `test_hydrostatics.c` (writing `test_hydro.csv` to `/tmp`) create their own controlled files at runtime so no stale disk file can corrupt the result.
- **Counting-macro style** = "run every check, then report the score" — `test_hydrostatics.c`'s custom `ASSERT` / `ASSERT_NEAR` increment `tests_run` and `tests_passed` rather than aborting on the first failure, so you see all failures at once instead of fixing them one by one.
- **`make test-asan`** = "repeat the whole suite with a memory watchdog enabled" — AddressSanitizer intercepts every heap read and write; if the `ship->cargo = NULL` line were ever removed from `parser.c`, the UAF regression test would trigger an ASan `heap-use-after-free` report with exit code 134.

**Why it matters:** a test that only checks a return value can miss a bug that returns the right code while leaving the `Ship` struct in a corrupt state — exactly the class of error the UAF regression test was written to catch; skipping `make test-asan` before touching `parser.c` or `analysis.c` means memory errors stay invisible until a fuzzer or a production crash finds them.

---

## What a unit test actually is

A unit test is a small program that calls one function with known inputs and checks that the output matches what you expect. If the check fails the program exits non-zero, and your build system knows the test failed.

In C the simplest possible check is `assert(condition)` from `<assert.h>`. If `condition` is zero (false), `assert` prints the file name, line number, and the condition text, then calls `abort()`. The OS then returns a non-zero exit code. That is the entire mechanism — no framework needed.

The contract is: **a test binary must exit 0 on success and non-zero on failure.** `make test` enforces this by running each binary in turn; the first non-zero exit stops the build.

---

## The two test files

CargoForge-C ships 8 test binaries (see §6.2 of the project digest). Two of them are studied here:

| Binary | Source | What it covers |
|---|---|---|
| `test_parser` | [`tests/test_parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/tests/test_parser.c) | Ship config parsing, cargo manifest parsing, UAF regression |
| `test_hydrostatics` | [`tests/test_hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/tests/test_hydrostatics.c) | CSV table loading, linear interpolation, boundary clamping |

---

## `test_parser.c` — assert-based style

The parser test uses the raw `assert()` macro directly. It is the simplest possible pattern: set up a scenario, call the function, assert the result.

```c
/* tests/test_parser.c — excerpt */
#include <assert.h>
#include <stdio.h>
#include "cargoforge.h"

int main() {
    printf("--- Running Parser Tests ---\n");
    Ship test_ship = {0};

    // Test 1: a bad config file must return -1.
    printf("Testing rejection of bad_ship.cfg...");
    int result = parse_ship_config("examples/bad_ship.cfg", &test_ship);
    assert(result == -1);
    printf(" OK\n");

    // Test 2: a good config file must return 0.
    printf("Testing acceptance of sample_ship.cfg...");
    result = parse_ship_config("examples/sample_ship.cfg", &test_ship);
    assert(result == 0);
    printf(" OK\n");
```

`Ship test_ship = {0}` is a common C idiom: it zero-initialises every field of the struct. The parser writes into this object; `= {0}` guarantees no field starts with a garbage value.

!!! note "What `assert` does at runtime"
    `assert(result == -1)` expands to something like:

    ```c
    if (!(result == -1)) {
        fprintf(stderr, "Assertion failed: result == -1, file tests/test_parser.c, line 19\n");
        abort();
    }
    ```

    `abort()` raises `SIGABRT`, which the OS turns into exit code 134. `make test` sees 134 ≠ 0 and stops.

---

## Test 3 — the UAF regression test

The third test in [`tests/test_parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/tests/test_parser.c) is the most important. It is a *regression test*: it exists precisely to prevent a bug from coming back. The bug was a heap-use-after-free in `parse_cargo_list`.

**The original bug.** When `parse_cargo_list` hit a bad field (e.g., a weight that was not a number), the old error path called `free(ship->cargo)` but left `ship->cargo_count` at its previous value and `ship->cargo` pointing at the now-freed memory. When `ship_cleanup` was later called, it iterated `for (int i = 0; i < ship->cargo_count; i++)` and accessed `ship->cargo[i].dg` — reading memory that had already been freed. AddressSanitizer names this **heap-use-after-free**.

**The fix.** Both error paths in `parse_cargo_list` now set `ship->cargo = NULL` and `ship->cargo_count = 0` immediately after the `free` call:

```c
/* parser.c — error path after free */
free(ship->cargo);
ship->cargo = NULL;    /* avoid a dangling pointer -> use-after-free in ship_cleanup */
ship->cargo_count = 0;
```

**The regression test.** The test constructs a manifest with one good line and one bad line (a non-numeric weight), calls `parse_cargo_list`, and then asserts the two invariants that block the bug:

```c
/* tests/test_parser.c — Test 3 */
printf("Testing cargo parse-error leaves no dangling pointer...");
{
    Ship s = {0};
    assert(parse_ship_config("examples/sample_ship.cfg", &s) == 0);
    const char *path = "build/_bad_cargo_test.txt";
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fputs("GoodItem 10 5x5x5 standard\nBadItem notanumber 5x5x5 reefer\n", f);
    fclose(f);
    assert(parse_cargo_list(path, &s) == -1); /* bad weight on line 2 */
    assert(s.cargo == NULL && s.cargo_count == 0); /* freed AND cleared */
    remove(path);
}
printf(" OK\n");
```

The test uses `FILE *f = fopen(path, "w")` to create a temporary file at runtime rather than shipping a fixture file on disk. This keeps the test self-contained: it creates exactly the input it needs, calls the function, checks the postcondition, then removes the file with `remove()`.

!!! warning "Why `assert(s.cargo == NULL && s.cargo_count == 0)` matters"
    Asserting only that `parse_cargo_list` returned `-1` would not be enough. The memory corruption happens *later*, when `ship_cleanup` reads the dangling pointer. The regression test pins down the **intermediate state** — the invariant that prevents the later crash — so that any future change that breaks the NULL-out will be caught immediately, not during fuzzing.

---

## `test_hydrostatics.c` — counting-macro style

The hydrostatics test uses a slightly more sophisticated pattern: a custom `ASSERT` macro that counts passes and failures rather than aborting on the first failure. This lets the test run to completion and report how many checks passed.

### The macro pair

```c
/* tests/test_hydrostatics.c */
static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol, msg) do { \
    tests_run++; \
    if (fabsf((a) - (b)) > (tol)) { \
        fprintf(stderr, "  FAIL: %s: %.4f != %.4f (tol %.4f) (line %d)\n", \
                msg, (float)(a), (float)(b), (float)(tol), __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)
```

`ASSERT_NEAR` is the key addition. Hydrostatic tables store and interpolate `float` values. Floating-point arithmetic is not exact: `1.0f / 2.0f` may not produce the bit-exact value `0.5f`. Comparing with `==` would give false failures. Instead, the test checks that the absolute difference is within a tolerance — here `0.01f` metres, which is 1 cm, much tighter than any real-world measurement uncertainty.

!!! note "The `do { ... } while(0)` wrapper"
    Wrapping the macro body in `do { ... } while(0)` makes it behave like a single statement. Without it, code like `if (x) ASSERT(y, "msg");` would silently mis-parse because the macro expands to multiple statements. This is a standard C macro hygiene pattern.

### The fixture helper

Rather than relying on a file on disk, the hydrostatics test writes its own CSV to `/tmp` at runtime:

```c
/* tests/test_hydrostatics.c */
static const char *write_test_csv(void) {
    static const char *path = "/tmp/test_hydro.csv";
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    fprintf(fp, "# Test hydrostatic table\n");
    fprintf(fp, "2.0,2000,10.00,1.00,9.00,8.00,180.0,3000,1.0\n");
    fprintf(fp, "4.0,4000,8.00,2.00,6.00,10.00,200.0,3500,0.5\n");
    fprintf(fp, "6.0,6000,7.00,3.00,4.00,11.00,220.0,3800,0.0\n");
    fprintf(fp, "8.0,8000,6.50,4.00,2.50,12.00,240.0,4000,-0.5\n");
    fclose(fp);
    return path;
}
```

This is a *fixture*: a helper that creates a known, controlled input. Using a hard-coded CSV means every test that calls `write_test_csv()` operates on identical data. If the format changes in a way that breaks parsing, this fixture is the first thing to update.

### What each sub-test asserts

```c
/* tests/test_hydrostatics.c — selected checks */

/* Parsing: correct row count and loaded flag */
ASSERT(table.count == 4, "4 entries parsed");
ASSERT(table.loaded == 1, "table marked as loaded");

/* Parsing: exact boundary values preserved */
ASSERT_NEAR(table.entries[0].draft, 2.0f, 0.01f, "first draft");

/* Rejection: non-ascending draft order must fail */
int ret = parse_hydro_table(path, &table);
ASSERT(ret == -1, "reject non-ascending draft");

/* Interpolation at an exact table point */
hydro_interpolate(&table, 4.0f, &result);
ASSERT_NEAR(result.displacement, 4000.0f, 0.1f, "exact displacement");
ASSERT_NEAR(result.kb, 2.0f, 0.01f, "exact KB");

/* Interpolation at the midpoint between 2.0 m and 4.0 m */
hydro_interpolate(&table, 3.0f, &result);
ASSERT_NEAR(result.displacement, 3000.0f, 0.1f, "midpoint displacement");
ASSERT_NEAR(result.kb, 1.5f, 0.01f, "midpoint KB");

/* Boundary clamping: draft below table range returns first entry */
hydro_interpolate(&table, 0.5f, &result);
ASSERT_NEAR(result.draft, 2.0f, 0.01f, "clamped to first entry");
```

The midpoint check is particularly valuable. The fixture rows at 2 m and 4 m have KB values of 1.00 m and 2.00 m respectively. Linear interpolation at 3 m must give exactly 1.50 m. If the interpolation logic were accidentally quadratic or had an off-by-one in the index arithmetic, this test would catch it.

The clamping tests assert that querying outside the table range does not crash or return garbage — it returns the nearest boundary entry. This is the *boundary condition* class of tests: inputs at or just beyond the edges of the valid domain.

### The main function pattern

```c
/* tests/test_hydrostatics.c */
int main(void) {
    printf("=== Hydrostatics Tests ===\n");

    test_parse_valid();
    test_parse_reject_unordered();
    test_parse_reject_too_few();
    test_interpolate_exact();
    test_interpolate_midpoint();
    test_interpolate_clamp_low();
    test_interpolate_clamp_high();
    test_draft_from_displacement();

    printf("Hydrostatics: %d/%d tests passed\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
```

The final line is the contract with `make`: return 0 only when every single check passed.

---

## What a good test asserts

Both test files illustrate the same three principles:

| Principle | Example from the codebase |
|---|---|
| **Assert the return value** | `assert(parse_ship_config(...) == -1)` — the function contract |
| **Assert the state after the call** | `assert(s.cargo == NULL && s.cargo_count == 0)` — the internal invariant |
| **Assert the computed value within tolerance** | `ASSERT_NEAR(result.kb, 1.5f, 0.01f, ...)` — numerical correctness |

A test that only checks the return value may miss a bug that returns the right code while leaving the object in a corrupt state (exactly what the UAF regression test guards against). A test that only checks one value may miss a bug in an adjacent field. Cover both.

---

## Running the tests

```
make test
```

This builds all 8 test binaries under `build/` and runs them in sequence. A clean run prints `OK` lines from each binary and exits 0. Any failing `assert` or non-zero exit causes `make` to stop and print the failing binary.

For memory-safety checks:

```
make test-asan
```

This rebuilds every source file with `-fsanitize=address,undefined` and re-runs the suite. AddressSanitizer intercepts every heap allocation and access; if the UAF regression test were to fail (e.g., after someone removed the `ship->cargo = NULL` line from `parser.c`), ASan would report `heap-use-after-free` and abort with exit code 134 — exactly how the original bug was discovered by [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh).

!!! tip "Test-asan is the authoritative check"
    `make test` catches logic errors. `make test-asan` catches memory errors that `make test` misses because freed memory is not always immediately re-used. Run `test-asan` before any commit that touches `parser.c`, `analysis.c`, or any function that allocates and then frees `Ship` or `Cargo` data.

---

## Recap

- `assert(condition)` from `<assert.h>` is the minimal building block: it crashes the program with a useful message and a non-zero exit code if the condition is false.
- A counting macro (`ASSERT` / `ASSERT_NEAR`) lets the test run to completion and report a tally, rather than stopping at the first failure — useful when you want to see all failures at once.
- `ASSERT_NEAR(a, b, tol, msg)` is mandatory for floating-point: KB, BM, displacement, and draft are all `float`, and exact equality comparisons produce false failures.
- Fixtures — small helper functions that write a known file to `/tmp` — keep test data self-contained and version-controlled without shipping large test files.
- Regression tests pin down *invariants*, not just return values: the UAF test asserts `s.cargo == NULL && s.cargo_count == 0` because that intermediate state is what prevents the later crash.
- `make test` checks logic; `make test-asan` checks memory safety. Both must pass before a change is considered correct.

*Next: [Continuous integration](16-continuous-integration.md).*
