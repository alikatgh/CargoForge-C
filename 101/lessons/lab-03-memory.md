# Lab 3 — Reproduce the Use-After-Free

This lab walks you through the exact memory bug that was found and fixed in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c). You will trigger it with AddressSanitizer, read the report, study the two-line fix, verify that the fixed code exits cleanly, and then run the project's fuzzer to confirm no similar bugs remain. When you finish, you will have a concrete mental model of what "heap-use-after-free" means and why NULL-ing a pointer after `free` is non-negotiable.

## Background

A **heap-use-after-free** occurs when code frees a block of heap memory and then reads or writes through a pointer that still points to that block. The memory manager may have already reused the block for something else — or left it zeroed — so the read returns garbage. Worse, with AddressSanitizer enabled, the allocator poisons freed memory and the program aborts the moment a stale pointer is dereferenced.

Here is the sequence that caused the bug in CargoForge-C:

1. `parse_cargo_list` encounters a bad weight field (`safe_atof` returns NaN).
2. The error path calls `free(ship->cargo)` — correct.
3. But it does **not** set `ship->cargo = NULL` or `ship->cargo_count = 0`.
4. The caller eventually invokes `ship_cleanup`, which iterates
   `for (int i = 0; i < ship->cargo_count; i++)` and touches `ship->cargo[i].dg`.
5. `ship->cargo_count` is still non-zero, so the loop runs — over the freed block.

ASan detects the access in step 5 and kills the process with exit code 134 (SIGABRT ≥ 128).

---

## Prerequisites

- You have completed Lab 1 and Lab 2 (the repo builds cleanly with `make`).
- You are at the repo root: `CargoForge-C/`.
- `gcc` (or `clang`) supports `-fsanitize=address,undefined`.

---

## Step 1 — Verify the ordinary build works

```bash
make
./cargoforge version
```

Expected output (version string may differ):

```
CargoForge v3.0.0
```

A clean ordinary build confirms you have a working baseline before adding sanitizers.

---

## Step 2 — Build the ASan binary

The fuzzer builds its own ASan binary at `build/cargoforge-asan`. You can also build it manually, which is exactly what [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) does on first run:

```bash
cc -O1 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   src/*.c -lm -o build/cargoforge-asan
```

!!! note
    `-O1` (not `-O3`) keeps the compiler from optimizing away the stale pointer
    access before ASan gets a chance to catch it. `-g` adds debug symbols so
    the ASan report shows file names and line numbers.

When the compilation finishes you will see the binary at `build/cargoforge-asan`. No output means success.

---

## Step 3 — Create a minimal ship config

The `validate` subcommand parses the ship config and the cargo manifest, then reports errors without running the full analysis. It still calls `ship_cleanup` on exit, which is what triggers the dangling-pointer access.

```bash
cat > /tmp/lab_ship.cfg << 'EOF'
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
EOF
```

---

## Step 4 — Create a manifest with a bad weight

The bug fires when `safe_atof` rejects the weight field. The value `abc` is not a number, so it will return NaN and trigger the error path:

```bash
cat > /tmp/lab_cargo_bad.txt << 'EOF'
# One valid item, one item with a bad weight
BoxA   100    10x5x3   standard
BoxB   abc    10x5x3   standard
EOF
```

`BoxA` parses successfully — `ship->cargo_count` becomes 1 and `ship->cargo` is a live pointer. When the parser hits `BoxB`, it detects the bad weight, frees `ship->cargo`, but (in the buggy version) leaves `ship->cargo_count = 1`. `ship_cleanup` then walks the freed array.

---

## Step 5 — Trigger the bug with ASan

Set the ASan options so the sanitizer aborts immediately on the first error, then run `validate`:

```bash
export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stacktrace=1"

build/cargoforge-asan validate /tmp/lab_ship.cfg /tmp/lab_cargo_bad.txt
echo "Exit code: $?"
```

!!! note "What you are looking at — the current (fixed) code"
    The bug was **already fixed** before this repo was published. The fix is in
    [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) lines 336–337 and 364–365. So the command above will exit
    cleanly with a non-zero error code (parse failure) but **no ASan report**.
    The steps below show you the exact two lines responsible for the fix, and the
    Solution section walks you through reverting them to reproduce the original crash.

Expected output from the **fixed** code:

```
Error: invalid weight value 'abc' for field 'weight'
Exit code: 1
```

No `AddressSanitizer` banner. No SIGABRT. Exit code 1 means "input rejected" — a clean error, not a crash.

---

## Step 6 — Examine the fix in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)

Open [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) and find the weight-parse error path around line 333:

```c
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;    // avoid a dangling pointer -> use-after-free in ship_cleanup
    ship->cargo_count = 0; // avoid iterating over freed memory
    /* ... cleanup stdin buffer or fclose ... */
    return -1;
}
```

The two lines marked are the entire fix. Without them:

- `ship->cargo` still holds the address of freed memory.
- `ship->cargo_count` is still 1.
- `ship_cleanup` iterates `cargo[0].dg` — reads from a freed block — and ASan kills the process.

The identical pattern appears again at the dimension-parse error path (around line 362) for the same reason: any error path that calls `free(ship->cargo)` must immediately NULL the pointer and zero the count.

!!! tip "The general rule"
    After `free(ptr)`, always write `ptr = NULL`. After freeing an array whose
    length is tracked separately, zero the length field immediately after. These
    two lines together make every downstream code path safe, regardless of what
    it checks or does not check.

---

## Step 7 — Run the full test suite under ASan

`make test-asan` rebuilds everything with `-fsanitize=address,undefined` and then runs all 8 test binaries:

```bash
make test-asan
```

Expected final lines:

```
--- Running All Tests ---
...
=== Memory safety tests (ASAN/UBSAN) passed ===
```

All 8 test binaries pass without any ASan report. This confirms that no other code path in the engine has the same class of bug.

---

## Step 8 — Run the fuzzer

The fuzzer ([`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) automates exactly what you did manually in Steps 2–5 — but at scale. It generates 300 randomly malformed ship configs and cargo manifests (including bad weights, bad dimensions, invalid DG fields, empty fields, overflow values) and feeds them to `build/cargoforge-asan`. Any exit code ≥ 128 or any line in stderr matching `AddressSanitizer`, `runtime error:`, or `UndefinedBehavior` is a FAIL.

```bash
make fuzz
```

The script builds the ASan binary if it does not already exist (you built it in Step 2, so it will skip the compile step). Each of the 300 iterations prints nothing on success. A passing run ends with:

```
Fuzz OK: 300 iterations, no crashes or sanitizer errors.
```

The fuzzer is how this bug was originally discovered: one of the adversarial `VALS` entries (`abc`, `0.0`, `-5`, etc.) triggered the bad-weight path, and ASan reported the use-after-free on the subsequent `ship_cleanup` call.

---

## Solution

To reproduce the original crash you need to temporarily remove the two fix lines, rebuild the ASan binary, and re-run Step 5. Here is the exact procedure:

**1. Remove the NULL-out lines from both error paths in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c).**

In the weight-parse error path (≈ line 336–337), delete:

```c
ship->cargo = NULL;
ship->cargo_count = 0;
```

In the dimension-parse error path (≈ line 364–365), delete the same two lines.

**2. Rebuild the ASan binary (force a rebuild by removing the old one):**

```bash
rm build/cargoforge-asan
cc -O1 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   src/*.c -lm -o build/cargoforge-asan
```

**3. Run Step 5 again:**

```bash
build/cargoforge-asan validate /tmp/lab_ship.cfg /tmp/lab_cargo_bad.txt
echo "Exit code: $?"
```

You will now see an ASan report similar to:

```
=================================================================
==XXXXX==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 8 at 0x... thread T0
    #0 0x... in ship_cleanup src/analysis.c:...
    #1 0x... in parse_cargo_list src/parser.c:...
...
Exit code: 134
```

Exit code 134 = SIGABRT (128 + 6). The fuzzer's `rc >= 128` check catches exactly this.

**4. Restore the fix:**

```c
free(ship->cargo);
ship->cargo = NULL;
ship->cargo_count = 0;
```

Apply to both error paths, rebuild, and confirm Step 5 exits with code 1 and no ASan output.

---

## Recap

- A **heap-use-after-free** happens when freed memory is later accessed through a stale pointer. C gives you no runtime protection — without a tool like ASan, the access silently reads garbage.
- The bug in `parse_cargo_list` left `ship->cargo` pointing to freed memory and `ship->cargo_count` non-zero. The cleanup function trusted the count and dereferenced the stale pointer.
- The fix is two lines: `ship->cargo = NULL; ship->cargo_count = 0;` — applied immediately after every `free(ship->cargo)` on an error path.
- `make test-asan` rebuilds the whole codebase with AddressSanitizer and UBSan, then runs the test suite. A clean run is a strong signal the codebase is free of this class of bug.
- `make fuzz` (via [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) generated 300 adversarial inputs including `abc` as a weight value, which is what triggered the original bug report. A passing fuzz run means the parser rejects all malformed input without crashing.

*Next: [Make and CMake](14-make-and-cmake.md).*
