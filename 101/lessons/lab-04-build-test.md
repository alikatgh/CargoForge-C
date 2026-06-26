# Lab 4 - Run and extend the tests

This lab turns the test theory from the surrounding lessons into muscle memory. You
will build CargoForge-C's eight test binaries, read one of them closely enough to
understand what it actually asserts, add your own assertion, watch it pass — and
then deliberately break a value to see what failure looks like. You will finish by
re-running the full suite under AddressSanitizer so the toolchain itself becomes
familiar.

All commands run from the repository root (`CargoForge-C/`).

---

## Part 1 — Build and run the full test suite

### Step 1 — Start from a clean state

```bash
make clean
```

This removes `build/`, the eight test binaries under `tests/`, and the main
`cargoforge` binary. A clean build confirms you are not relying on stale objects.

### Step 2 — Build everything and run all tests

```bash
make test
```

The Makefile builds every object under `build/`, links eight test binaries, and
runs them in sequence. You should see output that ends like this:

```
--- Running All Tests ---
./tests/test_parser
Error: Invalid or out-of-range length_m value 'abc'
Error: Invalid or out-of-range weight value 'notanumber'
--- Running Parser Tests ---
Testing rejection of bad_ship.cfg... OK
Testing acceptance of sample_ship.cfg... OK
Testing cargo parse-error leaves no dangling pointer... OK
--- All Parser Tests Passed ---
./tests/test_analysis

=== Running Analysis Module Tests ===

Test 1: Empty ship analysis... PASS
...
Test 10: Hydrostatic field values... PASS

=== All Analysis Tests Passed! ===

...

36 / 36 tests passed
All tests passed.
-----------------------
```

!!! note
    The two `Error:` lines near the top are expected — they come from
    `tests/test_parser`, which deliberately feeds bad input to the parser
    (`examples/bad_ship.cfg` contains `length_m=abc`). The test asserts that the
    parser returns `-1` on that bad input, so seeing the error message is proof
    the parser's validation fired correctly.

---

## Part 2 — Read a test file

Open `tests/test_analysis.c` in your editor. It is one of the simplest tests to
read because each test case is its own function.

Look at **Test 10** (`test_hydrostatic_fields`). It creates a ship in memory —
not by reading a file, but by setting fields directly in C — loads one cargo
item, calls `perform_analysis`, and then checks the identity
$GM = KB + BM - KG$:

```c
/* from tests/test_analysis.c, Test 10 */
float expected_gm = r.kb + r.bm - r.kg;
assert(fabsf(r.gm - expected_gm) < 0.01f);
```

This is not arbitrary. The formula $GM = KB + BM - KG$ is the fundamental
metacentric height equation (covered in Lesson 23). The test is enforcing that
`perform_analysis` in `src/analysis.c` computes these three quantities
consistently with one another — a mistake in any one of them would break this
assertion.

!!! tip
    Notice the pattern: tests in CargoForge-C do not read real ship files;
    they build `Ship` structs in memory. That keeps each test self-contained and
    fast, and it means the test describes exactly what the code receives — no
    file parsing involved.

---

## Part 3 — Add an assertion

You will add one assertion to `test_hydrostatic_fields`. The assertion checks
something the existing test does not: that `KB` stays below half the draft. For
a box hull with coefficient 0.53, the vertical centre of buoyancy should always
be less than the waterline.

Open `tests/test_analysis.c` and find the end of `test_hydrostatic_fields`,
just before the `free(ship.cargo)` call. Add one line:

```c
/* KB must be below half draft for a box-hull ship */
assert(r.kb < r.draft / 2.0f + 0.01f);
```

The `+ 0.01f` gives a small tolerance for floating-point rounding. Save the
file, then re-run just that test to confirm the assertion passes:

```bash
make tests/test_analysis && ./tests/test_analysis
```

Expected output (unchanged from before):

```
=== Running Analysis Module Tests ===

Test 1: Empty ship analysis... PASS
...
Test 10: Hydrostatic field values... PASS

=== All Analysis Tests Passed! ===
```

### Step 3b — Watch it fail

Now deliberately change the threshold to something impossible. Replace the line
you just added with:

```c
assert(r.kb < -1.0f);   /* force a failure */
```

Re-build and run:

```bash
make tests/test_analysis && ./tests/test_analysis
```

You will see output like this:

```
=== Running Analysis Module Tests ===

Test 1: Empty ship analysis... PASS
...
Test 10: Hydrostatic field values... Assertion failed: (r.kb < -1.0f), function test_hydrostatic_fields, file tests/test_analysis.c, line 263.
Abort trap: 6
```

The exact wording varies by OS (`Aborted` on Linux, `Abort trap: 6` on macOS),
but the key detail is the file name and line number — C's `assert()` macro
prints both automatically. The suite stops at the first failing assertion; any
test after this one would not run.

!!! warning
    `assert()` calls `abort()`. In a running binary that would be a crash.
    That is intentional for tests — a failing assertion should be loud and
    unmissable. In production code you use explicit error returns instead (see
    Lesson 8 and `safe_atof` in `src/parser.c`).

Restore the correct assertion before continuing:

```c
assert(r.kb < r.draft / 2.0f + 0.01f);
```

---

## Part 4 — Run the suite under AddressSanitizer

`make test-asan` rebuilds everything from scratch with two extra compiler flags:

- `-fsanitize=address` — AddressSanitizer (ASan): detects heap overflows,
  use-after-free, and double-free at runtime.
- `-fsanitize=undefined` — UndefinedBehaviorSanitizer (UBSan): detects signed
  integer overflow, null pointer dereference, and misaligned memory access.

```bash
make test-asan
```

Because `test-asan` starts with `clean`, compilation takes longer. You will see
the same test output as before, followed by:

```
=== Memory safety tests (ASAN/UBSAN) passed ===
```

If any test triggered a memory error, ASan would print a detailed report — the
kind that led to finding and fixing the heap-use-after-free in
`parse_cargo_list` described in the digest.

!!! note "Why ASan is not the default"
    Sanitized binaries run roughly 2× slower and use more memory. The default
    `make test` uses optimised (`-O3`) code that ships. `make test-asan` is
    reserved for safety audits and CI pipelines.

---

## Part 5 — Run the CLI against the sample inputs

The tests verify individual modules in isolation. The CLI exercises the whole
pipeline end to end. Run both sample inputs:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

This reads `examples/sample_ship.cfg` (a 150 m ship, 50 000 t capacity), places
the five cargo items from `examples/sample_cargo.txt`, runs `perform_analysis`,
and prints the human-readable loading plan.

Try the `validate` subcommand on the bad ship config to see the parser's error
handling in action:

```bash
./cargoforge validate examples/bad_ship.cfg examples/sample_cargo.txt
```

Expected:

```
Error: Invalid or out-of-range length_m value 'abc'
```

The binary exits with a non-zero code and prints no stability results — the
parser rejected the input before any calculation was attempted.

---

## Solution

The complete change to `tests/test_analysis.c` for Part 3 is a single inserted
line inside `test_hydrostatic_fields`, immediately before `free(ship.cargo)`:

```c
/* KB must stay below half-draft for a box-hull ship (KB_FACTOR = 0.53) */
assert(r.kb < r.draft / 2.0f + 0.01f);
```

No other files need to change. After saving, `make tests/test_analysis &&
./tests/test_analysis` should end with `=== All Analysis Tests Passed! ===`.

The failing version (Step 3b) used `assert(r.kb < -1.0f)` and produced an
`Abort trap` at that line. Restoring the correct assertion and re-running
confirms the fix.

---

## Recap

- `make test` builds and runs eight test binaries in one command; all 36
  assertions must pass before the Makefile reports success.
- Test cases in this codebase construct `Ship` structs in memory — no file I/O
  — keeping each test fast and self-contained.
- `assert()` terminates the process immediately on failure and prints the exact
  file and line number; this is correct behaviour for a test binary.
- `make test-asan` recompiles with AddressSanitizer and UBSan, catching
  memory-safety bugs that optimised builds can hide.
- `./cargoforge validate` exercises the full parse path; a non-zero exit code
  and a printed error message confirm the parser's error handling is working.
- A deliberately broken assertion (`assert(r.kb < -1.0f)`) is the fastest way
  to learn what test failure looks like before you rely on the suite for real.

*Next: [Buoyancy and Archimedes](17-buoyancy-and-archimedes.md).*
