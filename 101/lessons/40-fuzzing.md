# Fuzzing

Fuzz testing answers a question that unit tests can't: what happens when the program receives
input nobody thought to write a test for? CargoForge-C ships a purpose-built fuzzer in
[`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) that bombards the parser and processing pipeline with random, malformed, and
adversarial inputs. When combined with memory sanitizers, fuzzing becomes one of the most
effective ways to find real bugs — and it found a real one in this codebase.

---

## What is Fuzzing?

A fuzzer is a program that generates many varied inputs and feeds them to the target program,
looking for crashes or undefined behaviour. The key insight is that **a crash on bad input is
always a bug**. A program may legitimately reject bad input with an error message and a
non-zero exit code; what it must never do is corrupt memory, dereference a freed pointer, or
receive a signal that kills it.

CargoForge-C's contract, stated explicitly in `fuzz.sh`:

> *"A clean error exit (input rejected) is a PASS — the contract is 'never crash on bad
> input', not 'accept everything'."*

This is the right framing for any production tool. Users make mistakes. Pipelines feed
unexpected data. The question is whether the software fails gracefully or catastrophically.

---

## Memory Sanitizers — the Fuzzer's Amplifier

Without tooling, a heap-use-after-free might not crash immediately. The freed memory might
still contain the old values; the access looks fine at runtime, and the bug hides. Memory
sanitizers make the invisible visible.

`fuzz.sh` compiles a dedicated binary with two sanitizers:

```bash
cc -O1 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
   -fsanitize=address,undefined -fno-omit-frame-pointer src/*.c -lm -o "$SAN"
```

| Flag | Sanitizer | Catches |
|---|---|---|
| `-fsanitize=address` | AddressSanitizer (ASan) | Heap-use-after-free, heap buffer overflow, use-after-return, double-free |
| `-fsanitize=undefined` | UndefinedBehaviorSanitizer (UBSan) | Signed integer overflow, null pointer dereference, misaligned access, out-of-bounds array index |
| `-fno-omit-frame-pointer` | — | Preserves the call stack in crash reports |
| `-O1` | — | Light optimisation — enough to inline some code, not enough to elide the bugs |
| `-g` | — | Debug symbols for readable stack traces |

Both sanitizers are configured to abort immediately on the first error:

```bash
export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stacktrace=1"
```

`abort_on_error=1` means the process receives `SIGABRT` (signal 6), giving an exit code of
134. That is ≥ 128, which the fuzzer's failure check catches:

```bash
if [ "$rc" -ge 128 ] || grep -qiE 'AddressSanitizer|runtime error:|UndefinedBehavior' "$err"; then
```

Exit code ≥ 128 in Unix means the process was killed by a signal (`128 + signal_number`).
Any such exit is a crash; the fuzzer records it as a failure.

---

## How `fuzz.sh` Generates Inputs

### Iteration structure

```bash
ITERS="${1:-300}"
RANDOM="${2:-1}"
```

The loop runs `ITERS` iterations (default 300). The seed for the shell's `RANDOM` built-in is
fixed by default to `1`, making every CI run **deterministic and reproducible** — the same
sequence of random numbers produces the same inputs each time. You can override both from the
command line: `scripts/fuzz.sh 500 42`.

Each iteration:

1. Generate a random ship config file (`gen_config`).
2. Generate a random cargo manifest file (`gen_manifest`).
3. Choose a subcommand (`optimize` 2/3 of the time, `validate` 1/3).
4. Optionally add `--format=json`.
5. Run the sanitised binary, capture stderr.
6. Check for crash or sanitiser output.

### Adversarial corpora

`fuzz.sh` defines corpora of values known to stress-test parsers:

```bash
CKEYS=(length_m width_m max_weight_tonnes lightship_weight_tonnes lightship_kg_m \
       permissible_sf_tonnes hydrostatic_table tank_config bogus_key)
VALS=(150 25 -5 0 0.0 abc 1e9 99999999999 3.14 "" "  " /nonexistent.csv)
TYPES=(standard bulk reefer hazardous "" oog)
DG=("DG:3.1:UN1203:A:F-E,S-D" "DG:5.1:UN1942:A:F-A,S-Q" "DG:::" "DG:9:UN9999:X:" "DG:abc" "")
```

These are not random bytes — they are **targeted adversarial values** chosen because they
expose common parser failure modes:

- `-5` and `0.0` — values that pass `atof` but fail range validation in `safe_atof`.
- `abc` and `""` — values that fail numeric conversion entirely.
- `1e9` and `99999999999` — overflow-adjacent values.
- `/nonexistent.csv` — a path that triggers `fopen` failure inside `parse_hydro_table`.
- `bogus_key` — an unknown key the config parser must ignore without crashing.
- `DG:::` and `DG:abc` — malformed DG fields that `parse_dg_field` must reject cleanly.

### Config generation

`gen_config` writes 0–8 key=value lines. For each, it randomly chooses one of four formats:

```bash
case $((RANDOM % 4)) in
    0) printf '%s=%s\n' "$k" "$v" >> "$cfg" ;;          # normal
    1) printf '%s\n' "$k" >> "$cfg" ;;                   # no '=' at all
    2) printf '#%s=%s\n' "$k" "$v" >> "$cfg" ;;          # comment line
    3) printf '%s=%s=%s\n' "$k" "$v" "$v" >> "$cfg" ;;  # double '='
esac
```

This exercises the config parser's line-splitting logic across multiple malformed shapes —
not just bad values, but bad structure.

### Manifest generation

`gen_manifest` similarly generates 0–8 lines with five possible formats:

```bash
case $((RANDOM % 5)) in
    0) printf 'ID%d %s %sx%sx%s %s %s\n' ... ;;  # full valid-ish line with DG
    1) printf 'ID%d %s\n' ... ;;                   # too few fields
    2) printf '# comment %d\n' ... ;;
    3) head -c $((RANDOM % 400 + 1)) /dev/urandom | tr -d '\0\n' >> "$manifest"; printf '\n' ;;
    4) printf 'ID%d %s %s %s\n' ... ;;             # bad dimension format
esac
```

Case 3 is particularly aggressive: raw binary data from `/dev/urandom`, stripped of null bytes
(which would terminate a C string early), presented as a cargo line. The parser must not crash
on it.

---

## The Real Bug: Heap-Use-After-Free in `parse_cargo_list`

Fuzzing found a real memory bug. Understanding it is the clearest illustration of why this
tooling exists.

### The bug

In `parser.c`, the function `parse_cargo_list` allocated `ship->cargo` on the heap, then
parsed each line. If a line had an invalid weight field — `safe_atof` returning `NAN` — the
error path freed the array:

```c
for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
free(ship->cargo);
/* BUG: ship->cargo still points at the freed memory */
/* BUG: ship->cargo_count still holds the old value   */
```

Later, when `ship_cleanup` was called during CLI teardown, it iterated:

```c
for (int i = 0; i < ship->cargo_count; i++)
    if (ship->cargo[i].dg) free(ship->cargo[i].dg);
free(ship->cargo);
```

This is a **heap-use-after-free**: `ship->cargo` was already freed, but the stale pointer and
non-zero `ship->cargo_count` caused `ship_cleanup` to read through it again. With ASan active,
this triggered an immediate abort (exit code 134 ≥ 128) — exactly the signal the fuzzer
detects.

The bug was present in **two error paths**: one for an invalid weight field, one for invalid
dimensions. Both needed the same fix.

### The fix

The fix in `parser.c` (lines 334–344 and 362–372) is minimal and precise:

```c
for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
free(ship->cargo);
ship->cargo = NULL;    /* avoid dangling pointer -> use-after-free in ship_cleanup */
ship->cargo_count = 0; /* prevent iteration over freed memory                     */
```

Setting `ship->cargo = NULL` immediately after `free` ensures that if `ship_cleanup` runs
next, its guard:

```c
if (ship->cargo) { ... }
```

evaluates to false and the cleanup is a no-op for the cargo array. Setting `cargo_count = 0`
adds a second layer: even if the NULL check were absent, the loop body would never execute.

!!! note "The NULL-after-free pattern"
    Setting a pointer to `NULL` immediately after `free` is a standard defensive idiom in C.
    It doesn't prevent the original use-after-free (that already happened), but it prevents
    *subsequent* accesses through the same pointer in any code path that runs later. Combined
    with NULL guards before dereferencing, it makes double-free and use-after-free much harder
    to trigger in error paths.

### Why this was hard to find without fuzzing

Unit tests for `parse_cargo_list` would typically test successful parses and perhaps a couple
of known-bad inputs. The bug required a specific sequence: a parse that partially succeeds
(allocates and populates some cargo), then encounters a bad field mid-way through, then
reaches `ship_cleanup` afterwards. That sequence is obvious to a fuzzer generating hundreds of
malformed manifests, but easy to miss when writing tests by hand.

---

## Running the Fuzzer

```bash
make test-asan        # recommended first: runs the unit test suite under ASan
scripts/fuzz.sh       # 300 iterations with seed 1 (default)
scripts/fuzz.sh 1000  # more thorough — 1000 iterations
scripts/fuzz.sh 300 7 # 300 iterations, seed 7
```

The fuzzer reports each failure with the exact config and manifest that triggered it:

```
FUZZ FAIL (iter 47, sub=optimize, exit 134):
--- config ---
max_weight_tonnes=0.0
--- manifest ---
ID12345 abc 5x5x5 hazardous
--- stderr ---
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free ...
```

This output is everything you need to reproduce the crash deterministically, reduce the input
to a minimal reproducer, and then fix the bug.

`make test-asan` in the Makefile compiles the full test suite with `-fsanitize=address,undefined`
and runs it, catching the same class of bugs in the unit tests before fuzzing. The fuzzer and
ASan test suite are complementary: unit tests verify known-good behaviour, the fuzzer explores
the unknown.

---

## The Sanitized Binary vs the Release Binary

`fuzz.sh` intentionally keeps the sanitized binary (`build/cargoforge-asan`) separate from
the release binary (`cargoforge`). ASan and UBSan add roughly 2× runtime overhead and
significant memory overhead — unsuitable for production but ideal for testing. The Makefile
`test-asan` target does the same separation: it rebuilds with sanitizer flags, runs the tests,
then you revert to the optimised release build for deployment.

!!! warning "Do not ship the sanitized binary"
    The ASan runtime carries large overhead and exposes internal instrumentation. Always use
    the release binary (`make all`) for deployment; use `make test-asan` and [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)
    only during development and CI.

---

## Recap

- Fuzzing floods the program with adversarial, random, and malformed inputs; any crash is a
  bug regardless of how invalid the input was.
- CargoForge-C's `fuzz.sh` generates randomised ship configs and cargo manifests drawn from
  adversarial corpora of values, including negative numbers, empty strings, non-existent paths,
  and malformed DG fields.
- AddressSanitizer and UBSanitizer instrument the binary to abort immediately when memory
  errors or undefined behaviour occur, making latent bugs visible as detectable crashes
  (exit code ≥ 128).
- The fuzzer found a real heap-use-after-free in `parse_cargo_list`: error paths freed
  `ship->cargo` without clearing the pointer or the count, so `ship_cleanup` later iterated
  through freed memory.
- The fix is two lines: `ship->cargo = NULL` and `ship->cargo_count = 0` immediately after
  `free`, preventing any downstream code from following the dangling pointer.
- Fixed seed (default `RANDOM=1`) makes fuzz runs deterministic and reproducible in CI.

*Next: [Coverage and benchmarks](41-coverage-and-benchmarks.md).*
