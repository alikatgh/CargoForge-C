# Lab 1 — Build and Run It

You have read about the toolchain, the data model, and the physics. Now you make the program run on your machine and read every line of what it tells you. By the end of this lab you will have compiled CargoForge-C from source, run the full optimization pipeline on a real ship configuration, interpreted the stability output, deliberately triggered a parse error, and run the test suite — all from the command line.

---

## Prerequisites

- A C99-capable compiler: `gcc` or `clang`. On macOS run `xcode-select --install` if `cc --version` returns nothing. On Debian/Ubuntu run `sudo apt install build-essential`.
- `make` and `git` (included in the above packages).
- About 5 minutes of terminal time.

---

## Step 1 — Clone and enter the repository

```bash
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
```

The working directory now contains:

| Path | What lives there |
|---|---|
| `src/` | All C source files (`parser.c`, `analysis.c`, `placement_3d.c`, …) |
| `include/` | Public headers (`cargoforge.h`, `libcargoforge.h`, …) |
| `examples/` | Sample ship configs and cargo manifests |
| `tests/` | Eight unit-test binaries (one per module) |
| `scripts/` | Helper scripts including the fuzzer |
| [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) | Single entry point for every build target |

---

## Step 2 — Compile

```bash
make
```

`make` runs the default target `all`, which compiles every `.c` file in `src/` with `-O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L` and links them into a single binary called `cargoforge` in the project root. All intermediate `.o` files land in `build/`.

**Expected output (last two lines):**

```
cc -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude -fPIC -c src/cli.c -o build/cli.o
cc -o cargoforge build/main.o build/parser.o build/analysis.o ...
```

Confirm the binary exists:

```bash
./cargoforge version
```

Expected:

```
CargoForge-C version 3.0.0
Build date: Jun 25 2026
Maritime cargo optimization engine
```

!!! note "Version number"
    `3.0.0` is the `CF_VERSION_STRING` defined in `libcargoforge.h`. It tracks the public library API, not the CLI itself.

---

## Step 3 — Inspect the input files

Before you run anything, read the two files you will feed to the program.

```bash
cat examples/sample_ship.cfg
```

```
# Ship Specifications
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data for stability calculations
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

This is a minimal ship config: no hydrostatic table, no tank CSV, no strength limits. `parse_ship_config` will read it as a key=value file. Weights in tonnes are multiplied by 1 000 before storing, so `max_weight_tonnes=50000` becomes `ship->max_weight = 50,000,000 kg`.

```bash
cat examples/sample_cargo.txt
```

```
# Cargo Manifest
# ID              Weight(t) Dimensions(LxWxH)   Type
HeavyMachinery    550       20x5x3              standard
SteelBeams        400       18x2x2              bulk
ContainerA        250       12.2x2.4x2.6        reefer
ContainerB        250       12.2x2.4x2.6        reefer
SmallCrate        50        2x2x2               general
```

Five items, whitespace-delimited. `parse_cargo_list` makes two passes over a file: first to count items (so it can `malloc` the `cargo` array to the right size), then to populate it. No DG fields here — none of these items will have a `dg` pointer.

---

## Step 4 — Run the optimizer and read every line

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

Work through the output section by section.

### 4a — Loading phase

```
[OK] Ship configuration loaded
[OK] Cargo manifest loaded
```

`parse_ship_config` and `parse_cargo_list` both returned 0 (success). If either returned -1 the program would have exited before the next line.

### 4b — 3D bin-packing

```
Running 3D bin-packing...
Note: Reefer ContainerA placed in ForwardHold (deck preferred)
...
3D Placement complete: 5/5 items placed
  ForwardHold: 1500000.0 / 15000001.0 kg (10.0% capacity)
  AftHold: 0.0 / 15000001.0 kg (0.0% capacity)
  Deck: 0.0 / 20000000.0 kg (0.0% capacity)
[OK] Optimization complete
```

`place_cargo_3d` sorted cargo by volume descending (First Fit Decreasing), then tried each item against three hard-coded bins:

| Bin | x start | Width | Max weight |
|---|---|---|---|
| `ForwardHold` | 0 m | 45 m (150 × 0.30) | 15 000 t (50 000 × 0.30) |
| `AftHold` | 105 m | 45 m | 15 000 t |
| `Deck` | 0 m | 150 m | 20 000 t (50 000 × 0.40) |

The `Note:` lines are advisory: reefer cargo prefers deck (for power connections) but the constraint is not a hard rejection. All five items fit into ForwardHold at 10 % capacity — a lightly loaded ship.

### 4c — Stability summary

```
Ship: 150.00 m x 25.00 m | Max Weight: 50000.00 t
Hydrostatics: Box-hull approximation
```

No `hydrostatic_table` key was in the config, so `ship->hydro` is NULL. Every hydrostatic quantity is calculated from simple geometry using fixed coefficients (`BLOCK_COEFF = 0.75`, `KB_FACTOR = 0.53`, `WATERPLANE_COEFF = 0.85`).

```
  Displacement          : 3500.00 t (7.0% of max)
  Draft                 : 1.21 m
```

$$\text{displacement} = \text{lightship} + \text{cargo} = 2000 + 1500 = 3500\ \text{t}$$

$$\text{displaced volume} = \frac{3500}{1.025\ \text{t/m}^3} = 3414.6\ \text{m}^3$$

$$\text{draft} = \frac{3414.6}{150 \times 25 \times 0.75} = 1.21\ \text{m}$$

At only 7 % of maximum weight this is an unusually light load — the ship is riding high.

```
  KG / KB / BM          : 2.20 / 0.64 / 48.62 m
  GM (transverse)       : 47.06 m | WARNING - Too stiff
```

$$KG = \frac{\text{lightship moment} + \sum w_i \cdot z_{cg,i}}{\text{displacement}}$$

$$KB = 0.53 \times 1.21 = 0.64\ \text{m}$$

$$BM = \frac{L B^3}{12 \times 0.85 \times V} = \frac{150 \times 25^3}{12 \times 0.85 \times 3414.6} = 48.62\ \text{m}$$

$$GM = KB + BM - KG = 0.64 + 48.62 - 2.20 = 47.06\ \text{m}$$

A GM of 47 m is physically real but impractical — a ship this stiff has an extremely short roll period and delivers a violent, uncomfortable motion in a seaway. It is a consequence of the extremely light load: with almost no cargo, the wide waterplane dominates BM while KG stays low. In practice you would ballast the ship.

!!! warning "IMO GM minimum vs. stiffness"
    The IMO minimum is $GM \geq 0.15\ \text{m}$. There is no upper limit in the criteria, but class societies impose limits on roll period for crew habitability. CargoForge-C reports the warning when $GM$ is unusually large but does not fail the IMO check.

```
  Trim (by stern)       : -2.458 m
  Heel                  : -11.00 deg
```

Negative trim means bow-down (forward of midship). Negative heel means port. Both reflect the non-uniform cargo placement: all cargo landed in ForwardHold at x = 0, far from midship, and skewed to port.

### 4d — IMO Intact Stability check

```
IMO Intact Stability (MSC.267/85)
  GZ at 30 deg          : 27.582 m  (min 0.200) OK
  Max GZ                : 816.343 m at 80 deg (min 25 deg) OK
  Area 0-30 deg         : 6.8088 m-rad (min 0.0550) OK
  Area 0-40 deg         : 12.7473 m-rad (min 0.0900) OK
  Area 30-40 deg        : 5.9383 m-rad (min 0.0300) OK
  Overall               : COMPLIANT
```

Six criteria from MSC.267(85), Part A, Chapter 2.2. Each is calculated from the wall-sided GZ formula:

$$GZ(\theta) = \sin\theta \left(GM + \frac{BM \cdot \tan^2\theta}{2}\right)$$

The values are implausibly large (GZ of 27 m at 30°) because GM itself is 47 m — again, a very stiff, lightly loaded ship. COMPLIANT is correct; it just reflects an unusual loading condition.

### 4e — ASCII layout

```
=== Top-Down Cargo Layout (ASCII) ===
Ship: 150.0m (L) x 25.0m (W)
Scale: # = cargo, . = empty

   +--------------------------------------------------------------------------------+
 0 |###########.....................................................................|
...
```

A top-down view of the ship; each character represents a cell in a scaled grid. The `#` characters cluster at the bow (x ≈ 0), exactly where ForwardHold starts.

### 4f — Cargo placement table

```
=== Cargo Placement Summary ===

Cargo ID        | Type       |   Weight | Position |     Dims | Status
----------------+------------+----------+----------+----------+------------
HeavyMachinery  | standard   |   550.0t | 0.0,0.0,-8.0 | 20.0x5.0x3.0 | Placed
...
Placement rate: 5/5 items (100.0%)
```

Positions are in metres from the hull origin: $(x, y, z)$ where $z = -8\ \text{m}$ is the floor of the hold (8 m below the keel datum). Items with `pos_x < 0` show `NOT PLACED` and are excluded from all stability calculations.

---

## Step 5 — Validate without placing

```bash
./cargoforge validate examples/sample_ship.cfg examples/sample_cargo.txt
```

Expected:

```
Validating ship configuration: examples/sample_ship.cfg
[OK] Ship configuration is valid

Validating cargo manifest: examples/sample_cargo.txt
[OK] Cargo manifest is valid

[OK] All validation checks passed!
```

`validate` calls `parse_ship_config` and `parse_cargo_list` but skips `place_cargo_3d` and `perform_analysis`. Use it in CI to reject malformed input before it reaches the optimizer.

---

## Step 6 — Trigger a parse error

The repository ships a deliberately broken config:

```bash
cat examples/bad_ship.cfg
```

```
# This file contains a non-numeric value for length
length_m=abc
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

```bash
./cargoforge validate examples/bad_ship.cfg examples/sample_cargo.txt
```

Expected (exit code 5):

```
Validating ship configuration: examples/bad_ship.cfg
Error: Invalid or out-of-range length_m value 'abc'
[ERROR] examples/bad_ship.cfg: Invalid ship configuration

Validating cargo manifest: examples/sample_cargo.txt
[OK] Cargo manifest is valid

[FAILED] Validation failed with 1 error(s)
```

`safe_atof` in `parser.c` calls `strtof("abc", ...)`, detects that the conversion failed, prints the error to stderr, and returns `NAN`. The caller checks for `NAN` and returns -1, which propagates to a non-zero exit code. The program never crashes — it rejects bad input cleanly.

---

## Step 7 — Run the test suite

```bash
make test
```

This builds 8 test binaries in `tests/` and runs them all. The final summary should read:

```
36 / 36 tests passed
All tests passed.
-----------------------
```

Each binary covers one module:

| Binary | Module |
|---|---|
| `test_parser` | `parser.c` — config and manifest parsing |
| `test_analysis` | `analysis.c` — stability calculations |
| `test_constraints` | `constraints.c` + `placement_3d.c` + `imdg.c` |
| `test_hydrostatics` | `hydrostatics.c` — table interpolation |
| `test_tanks` | `tanks.c` — free surface moments |
| `test_longitudinal_strength` | SWSF/SWBM calculations |
| `test_imdg` | `imdg.c` — segregation matrix |
| `test_library` | `libcargoforge.a` — full public API |

Notice that `test_parser` deliberately feeds `bad_ship.cfg` to `parse_ship_config` and verifies rejection. It also tests that a cargo parse error leaves no dangling pointer — that was the real heap-use-after-free bug documented in the digest.

---

## Step 8 — Run the sanitized test suite

```bash
make test-asan
```

This recompiles everything with `-fsanitize=address,undefined` and runs the same 8 binaries. AddressSanitizer instruments every heap allocation and access; UBSan instruments arithmetic and pointer operations. On a clean build you will see the same pass count.

If you saw a failure here, the output would contain a line like:

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
```

That is exactly what the fuzzer detected on the original codebase before the `ship->cargo = NULL` fix was applied.

!!! tip "When to use test-asan"
    Run `make test-asan` any time you modify `parser.c`, `placement_3d.c`, or any function that calls `free`. ASan has near-zero false positives; if it fires, the bug is real.

---

## Bonus — Request JSON output

Any subcommand accepts `--format=json`:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt --format=json
```

The JSON contains the full `Ship` struct, each `Cargo` item with its placed position, and the complete `AnalysisResult`. This is the same payload the JSON-RPC server (`cargoforge serve`) returns.

---

## Solution

If any step produced unexpected output, work through this checklist:

1. **`make` errors mentioning missing header**: you are not in the repo root or the `include/` symlink is broken. Run `ls include/cargoforge.h` to confirm.
2. **`./cargoforge: command not found`**: the binary is in the current directory; use `./cargoforge`, not `cargoforge`.
3. **`test_parser` fails with an ASan report**: you are on a system where the pre-existing fix was not applied. Check `git log --oneline src/parser.c` for the commit that adds `ship->cargo = NULL` after `free(ship->cargo)`.
4. **Trim or heel values differ slightly**: the box-hull model uses fixed coefficients; tiny floating-point differences between compiler versions are normal at the last decimal place.

Complete expected output for `./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt` — the key lines to confirm:

```
3D Placement complete: 5/5 items placed
  Displacement          : 3500.00 t (7.0% of max)
  Draft                 : 1.21 m
  GM (transverse)       : 47.06 m | WARNING - Too stiff
  Overall               : COMPLIANT
Placement rate: 5/5 items (100.0%)
```

---

## Recap

- `make` compiles all source under `-std=c99` into a single `cargoforge` binary.
- `cargoforge optimize` runs the full pipeline: parse → bin-pack → stability analysis → report.
- Every numeric field in a config or manifest passes through `safe_atof`, which rejects non-numeric input cleanly rather than crashing.
- With no hydrostatic CSV, the box-hull model computes draft, KB, and BM from ship geometry and three fixed coefficients.
- $GM = KB + BM - KG$; a very light load produces a large BM and therefore a very large GM.
- `make test` runs 36 unit tests; `make test-asan` reruns them with AddressSanitizer to catch memory bugs.

*Next: [Structs, enums, and the data model](05-structs-enums-the-data-model.md).*
