# Lab 9 - Turn on a hydro table and a tank

The box-hull model works anywhere — no data required — but it trades accuracy for convenience.
Real ships carry hydrostatic tables surveyed by the yard, and real stability calculations include
a free-surface correction for partially-filled tanks. In this lab you will switch both on,
read the numbers that change, and understand *why* they change.

All commands run from the repository root. The binary must be built first (`make`).

---

## What you need

| File | Role |
|---|---|
| [`examples/sample_ship.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship.cfg) | Minimal ship — no hydro table, no tanks |
| [`examples/sample_ship_full.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship_full.cfg) | Same ship plus `hydrostatic_table=` and `tank_config=` keys |
| [`examples/sample_hydro.csv`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_hydro.csv) | Nine-column CSV table: draft → displacement, KB, BM, TPC, MTC, … |
| [`examples/sample_tanks.csv`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_tanks.csv) | Five partially-filled tanks: ballast, double-bottoms, fuel, fresh water |
| [`examples/sample_cargo.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_cargo.txt) | Five general cargo items, total 1 500 t |

---

## Step 1 — Confirm the build

```bash
make
./cargoforge version
```

You should see a version line such as `CargoForge 3.0.0`. If `make` errors, check that you have
a C99-capable compiler (`cc --version`) and that you are in the repository root.

---

## Step 2 — Run with the minimal ship (box-hull, no tanks)

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

Look for the **Stability** block in the output:

```
Hydrostatics: Box-hull approximation
...
Stability
  KG / KB / BM          : 2.20 / 0.64 / 48.62 m
  GM (transverse)       : 47.06 m | WARNING - Too stiff
  Trim (by stern)       : -2.458 m
  Heel                  : -11.00 deg
```

!!! note "Box-hull numbers"
    Draft is reported as **1.21 m** because the code divides displaced volume by
    $L \times B \times C_B$ (block coefficient 0.75).  KB is $0.53 \times \text{draft}$.
    BM uses a rectangular second moment of area with a waterplane coefficient of 0.85.
    GM = 47 m is physically absurd for a real ship — that is the hallmark of the approximation.
    There are no tanks to correct for, so `free_surface_correction` is 0.

---

## Step 3 — Inspect the full config

Open [`examples/sample_ship_full.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship_full.cfg) in any text viewer:

```
# Hydrostatic table (enables table-based calculations)
hydrostatic_table=examples/sample_hydro.csv

# Tank configuration (enables free surface correction)
tank_config=examples/sample_tanks.csv

# Permissible longitudinal strength limits (from class society)
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

Two new keys activate two new subsystems. Every other field is identical to `sample_ship.cfg`.

Now look at the hydrostatic table itself:

```bash
head -5 examples/sample_hydro.csv
```

Expected:

```
# draft_m,displacement_t,KM_m,KB_m,BM_m,TPC_t_cm,MTC_t_m,waterplane_m2,LCB_m
2.0,2306,13.20,1.06,12.14,9.60,185.0,3375,1.5
3.0,3544,9.92,1.58,8.34,10.20,195.0,3570,1.2
4.0,4838,8.23,2.10,6.13,10.70,205.0,3720,0.9
5.0,6181,7.25,2.63,4.62,11.10,215.0,3840,0.6
```

Each row is one surveyed waterline. `parse_hydro_table` in `hydrostatics.c` reads this and stores
it in a `HydroTable` hanging off `ship->hydro`. When the ship is loaded,
`hydro_draft_from_displacement` inverse-interpolates draft from the computed displacement, then
`hydro_interpolate` reads back KB and BM at that draft.

And the tank file:

```bash
cat examples/sample_tanks.csv
```

Expected:

```
# id,length_m,breadth_m,height_m,pos_x_m,pos_y_m,pos_z_m,fill_fraction,density_t_m3
BallastFP,8.0,12.0,6.0,140.0,0.0,0.0,0.50,1.025
BallastDB_P,20.0,4.0,6.0,75.0,-10.0,0.0,0.75,1.025
BallastDB_S,20.0,4.0,6.0,75.0,10.0,0.0,0.75,1.025
FuelOil_1,10.0,6.0,5.0,30.0,-8.0,0.0,0.60,0.950
FreshWater,5.0,5.0,4.0,20.0,0.0,0.0,0.85,1.000
```

None of the tanks is completely empty or completely full — which is exactly when free-surface
effects exist. The forepeak ballast is 50 % full; both double-bottom tanks are 75 % full.

---

## Step 4 — Validate both configs parse cleanly

```bash
./cargoforge validate examples/sample_ship_full.cfg examples/sample_cargo.txt
```

Expected:

```
Validating ship configuration: examples/sample_ship_full.cfg
[OK] Ship configuration is valid

Validating cargo manifest: examples/sample_cargo.txt
[OK] Cargo manifest is valid

[OK] All validation checks passed!
```

`validate` calls `parse_ship_config` and `parse_cargo_list` but stops before analysis.
Use it whenever you edit a config to catch typos early.

---

## Step 5 — Run with the full ship and observe the differences

```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt
```

Find the **Stability** block again:

```
Hydrostatics: Table interpolation
...
Stability
  KG / KB / BM          : 2.13 / 2.08 / 6.21 m
  GM (transverse)       : 6.16 m | WARNING - Too stiff
  Free surface corr     : -0.339 m
  GM (corrected)        : 5.82 m
  Trim (by stern)       : -4.902 m
  Heel                  : -57.53 deg
```

And the **Longitudinal Strength** block, which only appears when `permissible_*` keys are set:

```
Longitudinal Strength
  Max Shear Force       : 1484 t
  Max Bending Moment    : 32376 t-m
  Status                : WITHIN LIMITS
```

---

## Step 6 — Understand every number that changed

Work through the differences one at a time.

### 6.1 Draft: 1.21 m → 3.96 m

The displacement of this load is about 3 500 t (lightship 2 000 t + cargo 1 500 t + tank liquid).
The box-hull formula divides displaced volume by $L \times B \times C_B$:

$$\text{draft}_{\text{box}} = \frac{\text{displacement}_t / 1.025}{L \times B \times 0.75}
= \frac{3413}{150 \times 25 \times 0.75} \approx 1.21 \text{ m}$$

The table knows this ship's actual hull form. At 3 500 t it interpolates between the 3.0 t and
4.0 t rows and returns a draft close to **3.96 m** — the real waterline for this vessel.

### 6.2 KB: 0.64 m → 2.08 m

Box-hull uses the constant $KB = 0.53 \times \text{draft}$.
At draft 1.21 m that gives 0.64 m.

The table row near 4 m draft has $KB = 2.10$ m. Interpolation for 3.96 m yields **2.08 m** —
far more accurate because it reflects the actual underwater hull shape.

### 6.3 BM: 48.62 m → 6.21 m

This is the most dramatic change.
BM = $I_T / \nabla$ where $I_T$ is the second moment of the waterplane area and $\nabla$ is
displaced volume.

The box-hull calculates $I_T = L \times B^3 / 12 \times 0.85$ at the *box-hull draft of 1.21 m*,
giving a huge $\nabla$ denominator that is too small (shallow draft → small volume).
The table's BM at 3.96 m draft is **6.21 m**.

$$GM = KB + BM - KG$$

| | KB | BM | KG | GM |
|---|---|---|---|---|
| Box-hull | 0.64 | 48.62 | 2.20 | **47.06 m** |
| Table | 2.08 | 6.21 | 2.13 | **6.16 m** |

A GM of 47 m would make the ship roll like a pendulum on a rigid pivot — comfortable for cargo
but impossible in reality. The table-corrected 6.16 m is still stiff but physically plausible
for a lightly loaded general-cargo vessel.

### 6.4 Free-surface correction: 0 → −0.339 m

When liquid in a partially filled tank shifts as the ship heels, the effect is identical to
raising the centre of gravity. `calculate_free_surface_moment` in `tanks.c` computes, for each
tank:

$$FSM = \rho \times \frac{l \times b^3}{12}$$

where $\rho$ is liquid density, $l$ is tank length, $b$ is breadth. Empty and full tanks
contribute zero (no free surface). The total FSM is divided by displacement to get a virtual
KG rise in metres:

$$FSC = \frac{\sum FSM}{\Delta}$$

With five partially-filled tanks the correction is **−0.339 m**, meaning the *effective* GM
drops from 6.16 m to **5.82 m**. The sign convention in the output shows `Free surface corr:
-0.339 m` (a reduction in stability). All subsequent criteria — GZ curve, heel angle — use
`GM_corrected`.

!!! warning "Why the correction always reduces stability"
    A free surface *always* makes the ship less stable. The liquid moving to the low side
    adds a heeling moment that partially opposes the righting lever. You cannot design your
    way out of it — you can only minimize it by using longitudinally divided tanks (smaller
    $b$) or by running tanks fully empty or fully pressed-up.

### 6.5 Longitudinal strength — a new output section

The minimal config has no `permissible_*` keys, so `ship->strength_limits` is NULL and the
check is skipped. The full config sets all three limits. `calculate_longitudinal_strength`
distributes weight and buoyancy across 20 hull stations, integrates to shear force, integrates
again to bending moment, and compares peak values to the class-society limits.

---

## Step 7 — Extract numbers as JSON for comparison

The `--format json` flag lets you diff runs programmatically.

```bash
./cargoforge optimize examples/sample_ship.cfg     examples/sample_cargo.txt --format json \
    2>/dev/null | python3 -c "
import json,sys; r=json.load(sys.stdin)['analysis']
print('BOX  draft={draft:.3f}  KB={kb:.3f}  BM={bm:.3f}  GM={gm:.3f}  FSC={free_surface_correction:.3f}  GMc={gm_corrected:.3f}'.format(**r))
"

./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt --format json \
    2>/dev/null | python3 -c "
import json,sys; r=json.load(sys.stdin)['analysis']
print('TABLE draft={draft:.3f}  KB={kb:.3f}  BM={bm:.3f}  GM={gm:.3f}  FSC={free_surface_correction:.3f}  GMc={gm_corrected:.3f}'.format(**r))
"
```

Expected output (values match to the third decimal):

```
BOX   draft=1.214  KB=0.643  BM=48.619  GM=47.061  FSC=0.000  GMc=47.061
TABLE draft=3.962  KB=2.080  BM=6.213   GM=6.162   FSC=0.339  GMc=5.823
```

The field `hydro_table_used` in the JSON is `false` for the first run and `true` for the second —
a reliable programmatic flag for which code path was taken.

---

## Step 8 — Run the unit tests

The test suite covers the hydrostatics interpolation and tank free-surface functions directly:

```bash
make test
```

The targets `test_hydrostatics` and `test_tanks` exercise `parse_hydro_table`,
`hydro_interpolate`, `calculate_free_surface_moment`, and `calculate_total_fsm`. A passing run
prints no output and exits 0 for each binary.

For memory-safety confidence:

```bash
make test-asan
```

This rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer before running the same suite.
Any heap overflow, use-after-free, or signed integer overflow will abort with a diagnostic.

---

## Solution

If your run does not match the expected numbers, work through this checklist:

1. **Wrong config file.** Double-check you used `sample_ship_full.cfg` (not `sample_ship.cfg`)
   for the table run. The line `Hydrostatics: Table interpolation` confirms it.

2. **CSV paths are relative to the working directory.** `sample_ship_full.cfg` contains
   `hydrostatic_table=examples/sample_hydro.csv`. If you run `./cargoforge` from a subdirectory,
   that path does not resolve. Always run from the repository root.

3. **Build is stale.** If you edited source files, re-run `make` before `./cargoforge`.

4. **JSON parse fails.** The `--format json` output goes to stdout; progress messages go to
   stderr. The `2>/dev/null` in Step 7 suppresses the progress lines so `python3` only sees
   JSON. Remove it if you want to see the progress messages alongside.

The corrected JSON fields for the full-config run are:

```json
"draft": 3.962,
"kb":    2.080,
"bm":    6.213,
"gm":    6.162,
"free_surface_correction": 0.339,
"gm_corrected": 5.823,
"hydro_table_used": true
```

---

## Recap

- Adding `hydrostatic_table=` to the ship config switches `perform_analysis` from the box-hull
  constant-coefficient path to `hydro_draft_from_displacement` + `hydro_interpolate`, yielding
  accurate draft, KB, and BM from the surveyed table.
- Adding `tank_config=` activates `calculate_virtual_kg_rise`, which subtracts the free-surface
  correction from GM; tanks at 0 % or 100 % fill contribute nothing; partial fill contributes
  $\rho \, l \, b^3 / 12 \div \Delta$.
- The wall-sided GZ curve and all six IMO criteria always use `gm_corrected`, never the raw GM.
- Longitudinal strength checks (`SWSF`, `SWBM`) only appear when the three `permissible_*` keys
  are present in the ship config.
- `make test-asan` is the fastest way to verify that a config change has not introduced a
  memory error in the parser or analysis path.

*Next: [Sanitizers (ASan/UBSan)](38-sanitizers.md).*
