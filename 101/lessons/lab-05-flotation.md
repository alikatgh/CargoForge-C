# Lab 5 — Compute a Draft by Hand

CargoForge-C's `perform_analysis` function calculates a ship's draft from first principles —
Archimedes' law applied to the vessel's weight and geometry. This lab makes you do the same
calculation on paper, then run the program and compare the two answers. By the time you finish,
you will know exactly what `draft` means in `AnalysisResult`, why the box-hull model and a
real hydrostatic table give different numbers, and where each constant in the source code comes
from.

---

## 0. Prerequisites

You need a working build. From the repository root:

```bash
make
```

The binary `./cargoforge` must exist before you start. If the build fails, revisit Lab 4.

---

## 1. The Scenario

You will use the files that ship with the repository:

| File | Role |
|---|---|
| [`examples/sample_ship.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship.cfg) | Ship dimensions — 150 m × 25 m, lightship 2 000 t |
| [`examples/sample_cargo.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_cargo.txt) | Five cargo items totalling 1 500 t |

Open both files and read the numbers. You will need them in step 2.

**Ship:**
```
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

**Cargo** (first three columns only — `ID`, `weight_tonnes`, `dimensions`):

| ID | Weight (t) |
|---|---|
| HeavyMachinery | 550 |
| SteelBeams | 400 |
| ContainerA | 250 |
| ContainerB | 250 |
| SmallCrate | 50 |

---

## 2. The Hand Calculation

Work through this sequence. Write each number down; you will check it against the program output.

### Step 1 — Total displacement

Displacement is the total mass the sea must support: lightship weight plus all cargo.

$$\text{displacement} = \text{lightship} + \sum \text{cargo}_i = 2000 + (550 + 400 + 250 + 250 + 50) = 3500 \text{ t}$$

### Step 2 — Displaced volume

Seawater density $\rho = 1.025 \text{ t/m}^3$ (the `SEAWATER_DENSITY` constant in `analysis.c`).

$$\text{displaced volume} = \frac{\text{displacement}}{\rho} = \frac{3500}{1.025} \approx 3415 \text{ m}^3$$

### Step 3 — Draft (box-hull model)

The box-hull model treats the underwater body as a rectangular block with a block coefficient $C_b = 0.75$ (the `BLOCK_COEFF` constant in `analysis.c`). The volume of a box hull at draft $T$ is:

$$V = L \times B \times T \times C_b$$

Solving for $T$:

$$T = \frac{V}{L \times B \times C_b} = \frac{3415}{150 \times 25 \times 0.75} = \frac{3415}{2812.5} \approx 1.21 \text{ m}$$

!!! note
    The block coefficient accounts for the fact that a ship's hull is not a perfect box
    — the bow and stern are tapered. A value of 0.75 is typical for a general cargo vessel.
    A box barge would use $C_b = 1.0$.

Keep your answer. The expected result is **approximately 1.21 m**.

---

## 3. Running the Tool — Box-Hull Mode

[`examples/sample_ship.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship.cfg) does **not** reference a hydrostatic table, so CargoForge-C will use
the box-hull fallback path. Run:

```bash
./cargoforge optimize \
    examples/sample_ship.cfg \
    examples/sample_cargo.txt
```

Scan the output for the line that begins `Draft:`. You should see something close to your
hand-calculated value.

!!! example "Expected output (excerpt)"
    ```
    Draft:              1.21 m
    ```

The value matches your hand calculation because the program follows exactly the same formula
you used above. If you want to see every intermediate result in JSON instead of the human
report, add `--format json`:

```bash
./cargoforge optimize \
    examples/sample_ship.cfg \
    examples/sample_cargo.txt \
    --format json | grep '"draft"'
```

Expected JSON fragment:
```json
"draft": 1.21,
```

---

## 4. What Changes with a Real Hydrostatic Table?

[`examples/sample_ship_full.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship_full.cfg) adds three lines to the same ship definition:

```
hydrostatic_table=examples/sample_hydro.csv
tank_config=examples/sample_tanks.csv
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

The table covers drafts from 2.0 m to 10.0 m. Look at the first two rows of
[`examples/sample_hydro.csv`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_hydro.csv):

```
# draft_m,displacement_t,KM_m,KB_m,BM_m,TPC_t_cm,MTC_t_m,waterplane_m2,LCB_m
2.0,2306, ...
3.0,3544, ...
```

Our total displacement is 3 500 t. That falls between the row for 2.0 m (2 306 t) and the row
for 3.0 m (3 544 t). The function `hydro_draft_from_displacement` in `hydrostatics.c` performs
linear interpolation:

$$T \approx 2.0 + (3.0 - 2.0) \times \frac{3500 - 2306}{3544 - 2306} = 2.0 + 1.0 \times \frac{1194}{1238} \approx 2.96 \text{ m}$$

Run the full-config version to verify:

```bash
./cargoforge optimize \
    examples/sample_ship_full.cfg \
    examples/sample_cargo.txt
```

!!! example "Expected output (excerpt)"
    ```
    Draft:              2.96 m
    ```

### Why are the two drafts so different?

| Model | Draft | Why |
|---|---|---|
| Box-hull ($C_b = 0.75$) | ~1.21 m | Formula assumes the actual waterplane is 75% of the bounding box. |
| Hydrostatic table | ~2.96 m | Table rows were generated for a real hull with a true waterplane area. At 2–3 m draft the actual waterplane (~3 375–3 570 m²) is much smaller than $L \times B = 3750 \text{ m}^2$. |

The table-based result is the physically accurate one. The box-hull model is a first-order
approximation — useful for quick checks or when no table is available, but it can be
significantly wrong at light loading (low drafts) where hull form matters most.

!!! warning
    Never use box-hull results for a real loading decision. The `hydro_table_used` field in
    `AnalysisResult` tells you which model was active: `1` = table, `0` = box-hull fallback.

---

## 5. Confirming with `info`

The `info` subcommand parses the inputs and prints a summary without running the stability
solver. It is a useful sanity check that your files are parsed correctly before you interpret
analysis results:

```bash
./cargoforge info \
    examples/sample_ship.cfg \
    examples/sample_cargo.txt
```

Verify that the reported total cargo weight matches your hand-calculated 1 500 t and the
item count is 5. The `info` subcommand will also flag if any cargo item has `pos_x = -1`
(unplaced), which would mean it was excluded from the displacement sum.

---

## 6. Running the Tests

The unit tests for `analysis.c` exercise the same displacement and draft formulas you used
above. Run them now:

```bash
make test
```

All 8 test binaries should pass. If you want the memory-safety check, which confirms there
are no dangling pointers or buffer overflows in the parser and analysis code:

```bash
make test-asan
```

This rebuilds everything with AddressSanitizer and UBSan. A clean run means every formula
and every free-path was exercised without error.

---

## Solution

**Step 1 — displacement:**
$$2000 + 550 + 400 + 250 + 250 + 50 = 3500 \text{ t}$$

**Step 2 — displaced volume:**
$$3500 \div 1.025 = 3414.6 \text{ m}^3$$

**Step 3 — box-hull draft:**
$$3414.6 \div (150 \times 25 \times 0.75) = 3414.6 \div 2812.5 = 1.214 \text{ m}$$

**Table-based draft (interpolated):**
$$2.0 + \frac{3500 - 2306}{3544 - 2306} = 2.0 + \frac{1194}{1238} = 2.964 \text{ m}$$

**Program commands:**

```bash
# Box-hull (no hydrostatic table)
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt

# Table-based
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt

# Parse check only
./cargoforge info examples/sample_ship.cfg examples/sample_cargo.txt

# Run the test suite
make test

# Run with memory-safety checks
make test-asan
```

## Recap

- **Displacement** = lightship weight + all placed cargo weight (in tonnes).
- **Draft** follows from displaced volume and hull geometry: $T = V \div (L \times B \times C_b)$ for the box-hull model.
- CargoForge-C uses `SEAWATER_DENSITY = 1.025` t/m³ and `BLOCK_COEFF = 0.75` as named constants in `analysis.c`.
- When `hydrostatic_table` is set in the ship config, `perform_analysis` calls `hydro_draft_from_displacement` to inverse-interpolate draft from the CSV table — a more accurate but data-dependent path.
- The `hydro_table_used` field in `AnalysisResult` (and the `--format json` output) tells you which model produced the result.
- The box-hull draft at light loading can be 2× lower than the table-based draft; never rely on it for operational decisions.

*Next: [Center of gravity (KG)](21-center-of-gravity.md).*
