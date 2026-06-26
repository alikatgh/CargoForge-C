# Lab 12 — A Realistic Voyage

You have spent eleven lessons learning C from the ground up and building intuition for CargoForge-C's internals. This lab ties it together. You will act as the cargo officer for a 150-metre general-cargo vessel, run the full planning pipeline — validation, optimization, JSON export, Markdown report — and then read the numbers the way a naval architect reads them, catching problems before the ship leaves the berth.

Work from the repository root. Every command is copy-paste ready.

---

## Step 1 — Build and verify

Make sure you have a clean, current binary.

```bash
make
./cargoforge version
```

**Expected output:**

```
CargoForge-C version 3.0.0
Build date: ...
Maritime cargo optimization engine
```

If `make` fails, check that your compiler supports C99 (`cc --version`) and that `include/` and `src/` are intact.

---

## Step 2 — Inspect the vessel

The repository ships two ready-made fixtures. `examples/sample_ship_full.cfg` is the *full* ship configuration: it references a hydrostatic table, a tank CSV, and class-society longitudinal-strength limits. Open it and read it before running anything.

```
# examples/sample_ship_full.cfg (key lines)
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
hydrostatic_table=examples/sample_hydro.csv
tank_config=examples/sample_tanks.csv
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

The ship displaces about 2 000 t light and can carry up to 50 000 t. The hydrostatic table (`examples/sample_hydro.csv`) covers drafts from 2.0 m to 10.0 m; the tank CSV (`examples/sample_tanks.csv`) declares five partially-filled ballast and fuel tanks whose free surfaces will reduce effective GM.

Now ask CargoForge-C for a quick pre-load summary:

```bash
./cargoforge info examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

**Expected output (abridged):**

```
Ship Information
================
Dimensions:
  Length: 150.00 m
  Width: 25.00 m
  Deck Area: 3750.00 m2

Weight:
  Max cargo: 50000.00 t
  Lightship: 2000.00 t
  Lightship KG: 8.00 m

Cargo Summary:
  Items: 6
  Total weight: 910.00 t
  Utilization: 1.8%
  Hazardous: 3
  Reefer: 1
```

Six items, three of which carry DG (Dangerous Goods) codes. The manifest is `examples/sample_cargo_dg.txt`.

---

## Step 3 — Validate before loading

Never optimize without validating first. Validation parses both files and reports structural errors (bad field counts, unknown types, invalid DG grammar) without running any physics.

```bash
./cargoforge validate examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

**Expected output:**

```
Validating ship configuration: examples/sample_ship_full.cfg
[OK] Ship configuration is valid

Validating cargo manifest: examples/sample_cargo_dg.txt
[OK] Cargo manifest is valid

[OK] All validation checks passed!
```

!!! tip "Why validate separately?"
    `optimize` also validates, but it runs bin-packing on top. When you are debugging a bad manifest you want a fast, noise-free parse check — `validate` gives you that without the placement churn.

---

## Step 4 — Run the full optimization

```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

The terminal output has four sections: placement log, load summary, stability analysis, and an ASCII top-down layout. Read each section before moving on.

**Placement log (key lines):**

```
Running 3D bin-packing...
Note: Reefer ContainerA placed in ForwardHold (deck preferred)
Constraint: OxidizerDrum too close to FlammableLiquid (IMDG Separated from: 2.6m < 6.0m)
...
3D Placement complete: 6/6 items placed
  ForwardHold: 910000.0 / 15000001.0 kg (6.1% capacity)
  AftHold: 0.0 / 15000001.0 kg (0.0% capacity)
  Deck: 0.0 / 20000000.0 kg (0.0% capacity)
```

!!! warning "IMDG constraint messages are not failures"
    Lines beginning `Constraint:` are rejected orientation trials. The planner tried placing `OxidizerDrum` (IMDG class 5.1) next to `FlammableLiquid` (class 3.1), found the edge-to-edge horizontal distance below the 6 m "Separated from" minimum, and moved to the next candidate space. All 6 items were ultimately placed. A true failure would show `X/6 items placed` with `X < 6`.

**Stability output (key lines):**

```
Stability
  KG / KB / BM          : 3.27 / 1.84 / 7.22 m
  GM (transverse)       : 5.80 m | WARNING - Too stiff
  Free surface corr     : -0.386 m
  GM (corrected)        : 5.41 m
  Trim (by stern)       : -3.038 m
  Heel                  : -62.24 deg

IMO Intact Stability (MSC.267/85)
  ...
  Overall               : COMPLIANT
```

You will interpret these numbers in Step 7. Continue for now.

---

## Step 5 — Export a JSON report

```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt \
    --format=json > /tmp/voyage_report.json
```

The JSON structure mirrors `AnalysisResult` from `cargoforge.h`. Inspect the stability block:

```bash
python3 -m json.tool /tmp/voyage_report.json | grep -A 20 '"hydrostatics"'
```

**Expected excerpt:**

```json
"hydrostatics": {
    "draft": 3.962,
    "kg": 2.132,
    "kb": 2.08,
    "bm": 6.213,
    "gm": 6.162,
    "free_surface_correction": 0.339,
    "gm_corrected": 5.823,
    "hydro_table_used": true
},
```

Notice `hydro_table_used: true` — the program used the real hydrostatic table from `examples/sample_hydro.csv` rather than the box-hull fallback. This matters: the box-hull formula uses a fixed block coefficient of 0.75 and `KB_FACTOR = 0.53`; the table gives you interpolated KB and BM from real displacement data.

---

## Step 6 — Export a Markdown report

```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt \
    --format=markdown > /tmp/voyage_report.md
```

Open the file. You should see a structured Markdown document with a cargo table, stability values, and an IMO compliance section. This is what you would hand to a surveyor or email to a port agent. The `--format=markdown` path is implemented in `src/cli.c` and calls `print_loading_plan` internally.

---

## Step 7 — Stability assessment

Now read the numbers. This is the work of a qualified officer; CargoForge-C gives you the values, but you must judge them.

### 7.1 GM: too stiff

The corrected GM is **5.41 m** (or 5.82 m before the free-surface correction). IMO requires $GM \geq 0.15$ m, so the ship passes easily — but a very high GM is a problem of a different kind. A stiff ship with $GM \gg 1$ m has a very short roll period:

$$T = \frac{2 \pi k}{\sqrt{g \cdot GM}}$$

where $k$ is the roll radius of gyration. A short, violent roll is uncomfortable for crew, damaging to cargo, and in extreme seas can cause dynamic instability. The terminal even flags this: `WARNING - Too stiff`.

**Root cause**: only 910 t of cargo on a 2 000 t lightship. The centre of gravity is very low, producing a large righting arm. The fix is to add ballast (raise weight to open decks or deck cargo) or accept the condition for a short coastal voyage.

### 7.2 Free-surface correction

The five tanks in `examples/sample_tanks.csv` are partially filled (50–85% fill fractions). Each partially-filled rectangular tank has a free-surface moment:

$$\text{FSM} = \rho \cdot \frac{l \cdot b^3}{12}$$

CargoForge-C sums these across all tanks in `calculate_total_fsm` (from `src/tanks.c`) and divides by displacement to get the virtual KG rise: **0.339–0.386 m** depending on which manifest is used. This reduces corrected GM from 5.82 m to 5.41 m — a 7% reduction. For a lightly loaded ship the effect is moderate. On a ship near the $GM = 0.15$ m threshold it could be the difference between compliant and non-compliant.

!!! note "Empty and 100%-full tanks contribute zero FSM"
    `calculate_free_surface_moment` returns 0 for any tank with `fill_fraction == 0` or `fill_fraction == 1`. A sealed, pressed-up ballast tank has no free surface; the liquid cannot slosh.

### 7.3 Trim: bow-up

Trim is reported as **-3.038 m** — negative means *by the head* (bow deeper). This is unusual and significant. The cause is that all cargo ended up in the forward portion of the ship (ForwardHold, near x = 0), pulling the LCG far forward of midship. The ship's trim formula is:

$$\text{trim} = \frac{LCG \times L}{GM_L}$$

where $LCG$ is the longitudinal centre of gravity measured from midship (positive = aft).

The practical concern: a strong bow-trim reduces rudder effectiveness, increases bow-wave resistance, and may breach freeboard forward in head seas. Before sailing, the officer would transfer ballast aft or rearrange cargo to bring trim within the acceptable band (typically ±0.5% of $L$, which here is ±0.75 m).

### 7.4 Heel: large and suspicious

The heel of **−62 deg** (port) is physically implausible for a loaded ship and flags a limitation of the current model: with only 910 t of cargo against a 2 000 t lightship, and all of that cargo concentrated near the port side of the forward hold, the transverse moment calculation produces an extreme transverse CG offset. The heel formula is:

$$\text{heel} = \arctan\!\left(\frac{TCG}{GM_{\text{corrected}}}\right)$$

Even with a healthy GM of 5.41 m, a large TCG offset drives heel toward 90°. In practice a ship would be flooded, capsized, or the cargo would shift well before this angle; the wall-sided GZ formula used by `gz_at_angle` is only valid up to moderate angles (~25–30°). The take-away: heel above ~10° should trigger a manual re-stow, not just a note in the report.

### 7.5 IMO criteria and longitudinal strength

All six GZ criteria pass comfortably because the high GM produces a large righting lever at every angle. Longitudinal strength is well inside limits (SWBM 52 707 t·m vs. permissible 120 000 t·m hog, 100 000 t·m sag).

---

## Step 8 — Run the test suite

Verify that the full test suite passes on your machine:

```bash
make test
```

You should see eight test binaries run and exit cleanly. Then run the suite again with AddressSanitizer and UndefinedBehaviorSanitizer enabled:

```bash
make test-asan
```

ASan catches heap overflows and use-after-free bugs. The double-NULL pattern you studied in `src/parser.c` — zeroing both `ship->cargo` and `ship->cargo_count` after `free()` — exists precisely because an earlier version failed this check. Both `make test` and `make test-asan` must pass before any voyage plan is considered trustworthy code.

---

## Step 9 — Run the fuzzer

The fuzzer stress-tests the parser against adversarial inputs. It verifies the contract: *the program must never crash on bad input*, regardless of what the manifest contains.

```bash
bash scripts/fuzz.sh
```

The script runs 300 iterations by default. Each iteration generates a random ship config and cargo manifest — including malformed DG fields, zero weights, overflow values, and non-existent CSV paths — and calls either `optimize` or `validate`. Any exit code ≥ 128 (signal kill) or ASan output means a crash was found.

**Expected output:**

```
[FUZZ] CargoForge-C Fuzz Tester
...
[PASS] Iteration N: exit=1 (clean rejection)
...
[FUZZ] Completed 300 iterations
[PASS] No crashes found
```

Exit codes of 1 are expected and correct: the parser rejects invalid input, returns an error, and exits cleanly. Only signal-killed processes (exit ≥ 128) are failures.

---

## Recap

- `validate` → `info` → `optimize` is the correct pre-sail sequence: check syntax first, inspect statistics, then commit to placement.
- `--format=json` and `--format=markdown` write machine-readable and human-readable reports from the same analysis pass.
- A high GM (> ~2.5 m for this vessel size) is not automatically safe: it produces a stiff roll that can damage cargo and crew in heavy weather.
- The free-surface correction is the quantity $\sum \rho l b^3/12 \;\div\; \Delta$. Pressing up tanks (filling to 100%) eliminates their contribution entirely.
- Extreme trim or heel in CargoForge-C output is a signal to re-stow, not just note. The wall-sided GZ formula loses accuracy above ~25°.
- `make test-asan` and `scripts/fuzz.sh` are the two quality gates that prove the parser never crashes on unexpected input — a property the heap-use-after-free fix in `parse_cargo_list` was specifically designed to guarantee.

---

## Solution

If any step produces unexpected output, work through these checks in order:

**Build fails**: confirm `cc -std=c99` works. Run `make clean` then `make`.

**`validate` reports errors on `sample_cargo_dg.txt`**: the file must be at `examples/sample_cargo_dg.txt` relative to the repository root. Run `ls examples/` to confirm.

**`optimize` shows `X/6 items placed` with X < 6**: this would be a genuine placement failure — all six items should place. Re-run with `--verbose` to see which item failed and why.

**`hydro_table_used: false` in JSON**: the path `examples/sample_hydro.csv` in `sample_ship_full.cfg` is relative to the working directory. Run `./cargoforge` from the repository root, not from a subdirectory.

**Heel of −62° concerns you**: it is correct given the input. The vessel is lightly loaded with all cargo in the forward port quadrant. Add transversely balanced cargo or split weight between port and starboard positions in the manifest to bring heel below 10°.

**`make test-asan` fails with a sanitizer report**: do not ignore it. Note the file and line cited in the ASan output, consult `docs/BUG_JOURNAL.md` for known patterns, and fix the underlying issue before proceeding.

**Fuzzer finds a crash**: the output will include the iteration number and the random seed (`RANDOM=1` is fixed). Re-run that iteration manually to reproduce, then inspect the generated files in `/tmp/` that the script leaves behind.

*Next: [Glossary](glossary.md).*
