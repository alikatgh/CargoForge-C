# Lab 7 — Break the parser (safely)

The CargoForge-C parser is designed around one iron rule: **bad input must be rejected cleanly, never crash the program.** In this lab you will feed malformed ship configs, broken manifests, and valid dangerous-goods lines to the real binary and observe how each case is handled. You will also trigger the test suite and the fuzzer to see how the codebase defends itself at scale. Everything you run here is non-destructive — you are probing an error-handling contract, not breaking anything.

---

## Prerequisites

You need a built binary. From the project root:

```bash
make
```

You should see `cargoforge` appear in the project root. Confirm:

```bash
./cargoforge version
```

Expected output (version string may vary):

```
CargoForge-C 3.0.0
```

---

## Step 1 — Baseline: a valid run

Before breaking things, establish what success looks like. Run the optimizer with the bundled example files:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

You will see a loading plan followed by stability results. Notice:

- Exit code is 0 (`echo $?` → `0`).
- All cargo items are listed with assigned positions.
- A GM value appears and an IMO compliance verdict is printed.

This is the green baseline. Keep a mental note of what a clean run looks like.

---

## Step 2 — Malformed ship config: unknown key

Create a temporary ship config with a bogus key:

```bash
cat > /tmp/bad_ship.cfg << 'EOF'
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
hyperdrive_speed=warp9
EOF
```

Run `validate` (the subcommand that parses without computing anything):

```bash
./cargoforge validate /tmp/bad_ship.cfg examples/sample_cargo.txt
```

Expected output (stderr contains an informational note; exit code is still 0 because the config parser silently skips unknown keys):

```
Ship config loaded successfully.
Cargo manifest loaded successfully.
Validation passed: no critical errors detected.
```

!!! note "Unknown keys are ignored"
    `parse_ship_config` tokenises each line on `=` and matches against a fixed set of known keys. A line that matches nothing is simply skipped — it does not abort parsing. This is a deliberate design choice: forward-compatible configs work with older binaries.

---

## Step 3 — Malformed ship config: out-of-range value

Now give `max_weight_tonnes` a negative value:

```bash
cat > /tmp/neg_ship.cfg << 'EOF'
length_m=150
width_m=25
max_weight_tonnes=-5000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
EOF

./cargoforge validate /tmp/neg_ship.cfg examples/sample_cargo.txt
```

Expected stderr output (and a non-zero exit code):

```
Error: Invalid or out-of-range max_weight_tonnes value '-5000'
Error: Failed to load ship configuration.
```

`safe_atof` is called with `min=0.1` and `max=1e9` for every numeric field. A value below `min` causes `safe_atof` to return `NAN` and print the error message above. The caller sees `NAN`, treats it as a fatal parse failure, and returns `-1`. The CLI reports the error and exits non-zero.

---

## Step 4 — Malformed ship config: non-numeric value

```bash
cat > /tmp/alpha_ship.cfg << 'EOF'
length_m=one_hundred_fifty
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
EOF

./cargoforge validate /tmp/alpha_ship.cfg examples/sample_cargo.txt
```

Expected:

```
Error: Invalid or out-of-range length_m value 'one_hundred_fifty'
Error: Failed to load ship configuration.
```

The `strtof` inside `safe_atof` sets its `end` pointer to `s` (the start of the string) when no digits are found. The check `end == s` catches this and returns `NAN`. The field name is embedded in the message, so you always know exactly which key failed.

---

## Step 5 — Malformed cargo manifest: bad weight field

Create a manifest where the second item has a non-numeric weight:

```bash
cat > /tmp/bad_cargo.txt << 'EOF'
# Cargo with a broken weight field
HeavyMachinery    550       20x5x3        standard
BrokenItem        abc       6x2x2         general
EOF

./cargoforge validate examples/sample_ship.cfg /tmp/bad_cargo.txt
```

Expected (error pinpoints the field; exit code non-zero):

```
Error: Invalid or out-of-range weight value 'abc'
Error: Failed to load cargo manifest.
```

!!! warning "Why this matters beyond cosmetics"
    When `safe_atof` returns `NAN` for the weight field, `parse_cargo_list` must clean up the cargo it already parsed and NULL out `ship->cargo`. The fix described in the lesson on memory bugs (lesson 13) is exactly here — without the `ship->cargo = NULL` and `ship->cargo_count = 0` after `free`, a later call to `ship_cleanup` would walk the freed memory. Try running the same command with `make test-asan` later and you will see AddressSanitizer would catch this if the fix were missing.

---

## Step 6 — Malformed cargo manifest: bad dimensions

```bash
cat > /tmp/baddims.txt << 'EOF'
HeavyMachinery    550       20x5x3        standard
PartialBox        100       6x2           general
EOF

./cargoforge validate examples/sample_ship.cfg /tmp/baddims.txt
```

Expected:

```
Error: Incomplete or invalid dimensions for cargo 'PartialBox' on line 2
Error: Failed to load cargo manifest.
```

The dimensions field `6x2` has only two components. `parse_cargo_list` splits on `x` and expects exactly three parts (L × W × H). The third `strtok_r` call returns `NULL`, `dims_ok` is set to false, and the same cleanup path runs as in Step 5.

---

## Step 7 — Malformed cargo manifest: too few fields

```bash
cat > /tmp/toofew.txt << 'EOF'
HeavyMachinery    550
EOF

./cargoforge validate examples/sample_ship.cfg /tmp/toofew.txt
```

Expected:

```
Warning: Skipping malformed cargo data on line 1.
Ship config loaded successfully.
Cargo manifest loaded successfully.
Validation passed: no critical errors detected.
```

!!! note "Warnings vs. errors"
    Lines with fewer than four fields produce a *warning* and are skipped, not a fatal error. The parser continues to the next line. This is by design: a manifest with one malformed line should not invalidate an entire shipment.

---

## Step 8 — Writing and parsing a valid DG line

Open `examples/sample_cargo_dg.txt` and study the format. Now write your own manifest with a single dangerous-goods item — flammable liquid (IMDG class 3, division 1, UN 1203, stow anywhere, EmS F-E,S-D):

```bash
cat > /tmp/dg_test.txt << 'EOF'
# DG test manifest
Gasoline   25   6x2.5x2.6   hazardous   DG:3.1:UN1203:A:F-E,S-D
EOF

./cargoforge validate examples/sample_ship.cfg /tmp/dg_test.txt
```

Expected (clean parse):

```
Ship config loaded successfully.
Cargo manifest loaded successfully.
Validation passed: no critical errors detected.
```

Now deliberately break the DG field — set the class to `0` (outside the valid 1–9 range):

```bash
cat > /tmp/dg_bad.txt << 'EOF'
Gasoline   25   6x2.5x2.6   hazardous   DG:0:UN1203:A:F-E,S-D
EOF

./cargoforge validate examples/sample_ship.cfg /tmp/dg_bad.txt
```

Expected output (item is loaded but not flagged as DG — `parse_dg_field` returns NULL for invalid class):

```
Ship config loaded successfully.
Cargo manifest loaded successfully.
Validation passed: no critical errors detected.
```

The item is still loaded as a `hazardous` type, but without a `DGInfo` struct (`c->dg` is NULL). The IMDG segregation engine will not apply class-based rules to it — only the legacy 3-metre hazmat separation applies. This is intentional: a malformed DG field degrades gracefully rather than rejecting the entire manifest.

!!! tip "Try the full DG pipeline"
    Run `optimize` (not `validate`) on `examples/sample_cargo_dg.txt` and look for the IMDG segregation section in the output. Each DG pair is checked against the IMDG Table 7.2.4 segregation matrix.

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo_dg.txt
```

---

## Step 9 — Run the unit test suite

The repository ships 8 focused test binaries. Run them all:

```bash
make test
```

Expected (all pass):

```
Running test_parser...       PASSED
Running test_analysis...     PASSED
Running test_constraints...  PASSED
Running test_hydrostatics... PASSED
Running test_tanks...        PASSED
Running test_longitudinal_strength... PASSED
Running test_imdg...         PASSED
Running test_library...      PASSED
All tests passed.
```

`test_parser` covers both the ship config and cargo manifest parsers directly, including error paths.

---

## Step 10 — Run the test suite under AddressSanitizer

```bash
make test-asan
```

This rebuilds every test binary with `-fsanitize=address,undefined` and re-runs the suite. If any test triggers a heap-use-after-free, stack overflow, signed integer overflow, or null-pointer dereference, ASan prints a detailed report and the build target fails.

Expected:

```
Rebuilding with AddressSanitizer + UBSan...
Running test_parser...       PASSED
...
All tests passed.
```

The null-out fix in `parse_cargo_list` (setting `ship->cargo = NULL` and `ship->cargo_count = 0` after freeing) is what makes this target pass on the error paths you exercised in Steps 5 and 6. Before the fix, `test_parser` would have triggered an ASan `heap-use-after-free` report for exactly those cases.

---

## Step 11 — The fuzzer

The fuzzer in `scripts/fuzz.sh` generates hundreds of random and adversarial ship configs and manifests — including zero weights, negative values, non-numeric strings, non-existent CSV paths, and malformed DG fields — and feeds them to the ASan binary. A crash or sanitizer trip is a failure; a clean error exit is a pass.

Run a short session (50 iterations to keep it fast):

```bash
bash scripts/fuzz.sh 50
```

Expected:

```
Building sanitized binary (build/cargoforge-asan)...
Fuzz OK: 50 iterations, no crashes or sanitizer errors.
```

!!! note "What the fuzzer tests"
    The fuzzer does not check that the parser produces *correct* output — it checks that it never *crashes*. An "Error: Invalid …" message on stderr with exit code 1 is a pass. Only exit codes ≥ 128 (signal kills) or ASan/UBSan lines on stderr are failures.

For the full 300-iteration CI run:

```bash
bash scripts/fuzz.sh
```

---

## Solution

Below are the key observations you should be able to articulate after working through this lab.

**What is the parser's contract?**
Every malformed input must produce either a clean error message (fatal parse failure → non-zero exit) or a warning (skipped line → parsing continues). The binary must never crash, segfault, or trip a memory sanitizer on any input.

**Three rejection categories:**

| Input type | Behaviour | Why |
|---|---|---|
| Out-of-range or non-numeric value | Fatal — `safe_atof` returns `NAN`, caller returns `-1` | Data corruption risk |
| Too few fields on a manifest line | Warning — line is skipped | Tolerates partial lines |
| Unknown config key | Silently ignored | Forward compatibility |

**The DG field grammar:**
`DG:<class>[.<division>]:<UNnumber>:<stowage>:<EmS>`. Class must be 1–9; anything outside that range causes `parse_dg_field` to return NULL, and the item loads without DG metadata.

**Why NULL-out after free?**
After `free(ship->cargo)` in an error path, the code sets `ship->cargo = NULL` and `ship->cargo_count = 0`. This prevents `ship_cleanup` — which iterates `cargo[0..cargo_count-1]` — from accessing freed memory. Without the NULL-out, the test-asan target would catch a heap-use-after-free on the error paths exercised in Steps 5 and 6.

**The fuzzer's pass condition:**
Exit code < 128 AND no `AddressSanitizer`/`runtime error:` text on stderr. A clean rejection (exit code 1, error message on stderr) is a pass.

---

## Recap

- `safe_atof` is the single chokepoint for all numeric fields; it validates range and rejects non-numeric strings before they reach any computation.
- Fatal parse errors free all partially-allocated cargo and NULL out the pointer before returning, preventing use-after-free in `ship_cleanup`.
- Manifest lines with too few fields are warned and skipped; they do not abort the entire parse.
- A malformed DG field (`DG:` prefix with invalid class) causes the item to load as generic hazmat, not as DG — graceful degradation rather than rejection.
- `make test-asan` rebuilds with AddressSanitizer and UBSan; it is the definitive proof that memory handling is correct on error paths.
- The fuzzer (`scripts/fuzz.sh`) drives this contract at scale: hundreds of adversarial inputs, none may crash the binary.

*Next: [The stowage problem](30-the-stowage-problem.md).*
