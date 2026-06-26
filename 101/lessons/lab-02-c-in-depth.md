# Lab 2 — Read the Data Model

This lab grounds the abstractions from Lessons 5 and 26 in something tangible: you will open
`include/cargoforge.h`, read the real `Ship` and `Cargo` structs, then trace every field from
a sample config file all the way into the struct. No writing code yet — just reading, running
commands, and checking that what you see matches what the program reports.

---

## Before You Start

Make sure the project builds cleanly:

```bash
cd /path/to/CargoForge-C
make
```

Expected last line: something like `gcc … -o cargoforge`. If it fails, check that you have `gcc`
or `clang` installed and that you are in the right directory.

---

## Step 1 — Open the header and read the structs

Open `include/cargoforge.h` in any text editor or viewer. Find the two typedef blocks beginning
around line 39 (`Cargo`) and line 53 (`Ship`).

**Checkpoint 1a — answer these questions before moving on:**

| Question | Where to look |
|---|---|
| What C type stores the cargo ID? How many characters maximum? | `Cargo.id` field |
| Weight is stored as `float` — in what unit? | Digest §1.1 (the parser converts tonnes × 1000) |
| What does `pos_x = -1.0` mean for a cargo item? | Comment above the `Cargo` typedef |
| What is the type of `Ship.cargo`? Is it a single item or multiple? | `Cargo *cargo` — a pointer to an array |
| What three optional sub-structs can be NULL on a `Ship`? | The "Optional data" block in the struct |

!!! note
    A field of type `struct HydroTable_ *` is a *pointer* to a struct defined elsewhere.
    When that pointer is `NULL` the feature is disabled and the code falls back to a simpler
    calculation. You do not need to understand the full struct yet — just note the pattern:
    **NULL means "not loaded."**

---

## Step 2 — Read the sample ship config

```bash
cat examples/sample_ship.cfg
```

Expected output:

```
# This is the correct content for examples/sample_ship.cfg

# Ship Specifications
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data for stability calculations
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

**Trace every key to a struct field.** Fill in the table:

| Config key | Value | Stored in | Unit stored |
|---|---|---|---|
| `length_m` | 150 | `Ship.length` | metres |
| `width_m` | 25 | `Ship.width` | metres |
| `max_weight_tonnes` | 50 000 | `Ship.max_weight` | **kg** (× 1000) |
| `lightship_weight_tonnes` | 2 000 | `Ship.lightship_weight` | **kg** (× 1000) |
| `lightship_kg_m` | 8.0 | `Ship.lightship_kg` | metres |

!!! warning "The tonnes/kg conversion is a source of bugs"
    The config file uses tonnes because that is the maritime convention. The struct stores
    kilograms because C arithmetic is easier in a single unit. Every time you see `_tonnes`
    in a key name, multiply by 1 000 mentally when reading the stored value. The function
    `parse_ship_config` in `src/parser.c` does this with the line
    `ship->max_weight = val * 1000.0f;`.

---

## Step 3 — Read the sample cargo manifest

```bash
cat examples/sample_cargo.txt
```

Expected output:

```
# Cargo Manifest
# ID              Weight(t) Dimensions(LxWxH)   Type
HeavyMachinery    550       20x5x3              standard
SteelBeams        400       18x2x2              bulk
ContainerA        250       12.2x2.4x2.6        reefer
ContainerB        250       12.2x2.4x2.6        reefer
SmallCrate        50        2x2x2               general
```

For the first item `HeavyMachinery`, map each token to the `Cargo` struct:

| Token | Field | Stored value |
|---|---|---|
| `HeavyMachinery` | `Cargo.id` | `"HeavyMachinery"` (char[32]) |
| `550` | `Cargo.weight` | 550 000.0 f (kg) |
| `20x5x3` | `Cargo.dimensions[0/1/2]` | 20.0, 5.0, 3.0 (metres) |
| `standard` | `Cargo.type` | `"standard"` (char[16]) |

The `pos_x`, `pos_y`, `pos_z` fields are not in the file. After parsing they will all be
`-1.0f` — the "unplaced" sentinel. The `dg` pointer will be `NULL` because there is no `DG:`
field.

**Checkpoint 3 — what are `Cargo.dimensions[0]`, `[1]`, and `[2]` for `ContainerA`?**

> 12.2, 2.4, 2.6 (metres — length, width, height)

---

## Step 4 — Run `info` and verify the parsed values

The `info` subcommand prints ship and cargo statistics without running the optimizer:

```bash
./cargoforge info examples/sample_ship.cfg examples/sample_cargo.txt
```

You should see a block similar to:

```
=== Ship Information ===
  Length:          150.00 m
  Width:            25.00 m
  Max weight:    50000.00 t
  Lightship:      2000.00 t
  Lightship KG:      8.00 m
  Hydrostatics:   box-hull fallback
  Tanks:          none

=== Cargo Manifest (5 items) ===
  HeavyMachinery   550.0 t  20.0x5.0x3.0 m  standard
  SteelBeams       400.0 t  18.0x2.0x2.0 m  bulk
  ...
```

**Checkpoint 4 — verify these three things:**

1. `Max weight` shows `50000.00 t` — that is `Ship.max_weight / 1000` printed back to the
   user in tonnes.
2. `Hydrostatics: box-hull fallback` — `Ship.hydro` is `NULL` because `sample_ship.cfg` does
   not set `hydrostatic_table`.
3. The cargo count says `(5 items)` — matches `Ship.cargo_count`.

---

## Step 5 — Trace one key through the source

Open `src/parser.c` and search for the string `"length_m"`. You will find a block like:

```c
if (strcmp(key, "length_m") == 0) {
    ship->length = safe_atof(value, 0.1f, 1e9f, "length_m");
```

This is the complete path:

```
examples/sample_ship.cfg          (file on disk)
  → parse_ship_config()           (reads key=value lines)
    → strcmp(key, "length_m")     (matches the key)
      → safe_atof(value, …)       (converts "150" to 150.0f, validates range)
        → ship->length = 150.0f   (stored in the Ship struct)
```

`safe_atof` validates the parsed float against `[min, max]` and returns `NAN` on failure,
printing an error. This is why bad input like `length_m=abc` causes a clean error message
rather than undefined behaviour.

**Checkpoint 5 — what would happen if you wrote `length_m=0` in the config?**

> `safe_atof` would reject it (minimum is 0.1) and print an error. `parse_ship_config` would
> return `-1`. The program would report a parse failure without crashing.

Try it:

```bash
echo "length_m=0" | ./cargoforge validate - examples/sample_cargo.txt
```

You should see an error mentioning `length_m` and a non-zero exit code — not a crash.

---

## Step 6 — The full config and optional fields

Now look at the full config:

```bash
cat examples/sample_ship_full.cfg
```

Expected output:

```
# Full ship configuration with hydrostatic table, tanks, and strength limits
# MV Example - 150m x 25m general cargo vessel

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

Three keys that were absent in `sample_ship.cfg` are now present:

| Config key | Effect on the `Ship` struct |
|---|---|
| `hydrostatic_table` | Parser calls `parse_hydro_table`; `Ship.hydro` is no longer NULL |
| `tank_config` | Parser calls `parse_tank_config`; `Ship.tanks` is no longer NULL |
| `permissible_sf_tonnes` + `permissible_bm_*` | Parser allocates `Ship.strength_limits`; no longer NULL |

Run `info` with the full config:

```bash
./cargoforge info examples/sample_ship_full.cfg examples/sample_cargo.txt
```

**Checkpoint 6 — compare `Hydrostatics:` line to Step 4.** It should now say something like
`table (N rows)` instead of `box-hull fallback`, confirming `Ship.hydro` was populated.

---

## Step 7 — Spot a DG field

```bash
cat examples/sample_cargo_dg.txt
```

Find the line:

```
FlammableLiquid   25        6x2.5x2.6           hazardous   DG:3.1:UN1203:A:F-E,S-D
```

The fifth token `DG:3.1:UN1203:A:F-E,S-D` is parsed by `parse_dg_field` in `src/parser.c`.
The grammar is:

```
DG : <class>.<division> : <UN number> : <stowage> : <EmS>
 3       3         1       UN1203          A          F-E,S-D
```

After parsing, `cargo->dg` points to a heap-allocated `DGInfo` struct with:
- `dg_class = 3`, `dg_division = 1`
- `un_number = "UN1203"` (gasoline)
- `stowage = STOW_ANY` (stowage category A)
- `ems = "F-E,S-D"` (emergency schedule)

For `HeavyMachinery` (no DG field), `cargo->dg` is `NULL`. This is how the code distinguishes
plain cargo from regulated dangerous goods without adding extra boolean fields.

**Checkpoint 7 — what value would `dg_class` hold if the field were `DG:5.1:UN1942:A:F-A,S-Q`?**

> `dg_class = 5`, `dg_division = 1`. IMDG Class 5.1 is oxidizing substances.

---

## Step 8 — Run validate on the DG manifest

```bash
./cargoforge validate examples/sample_ship.cfg examples/sample_cargo_dg.txt
```

A clean manifest produces output like:

```
Validation passed: 6 cargo items parsed successfully.
```

Exit code 0. Now introduce a deliberate error — a DG class outside the valid range (1–9):

```bash
printf 'BadDG 10 2x2x2 hazardous DG:99:UN0000:A:F-A,S-Q\n' \
  | ./cargoforge validate examples/sample_ship.cfg -
```

Expected: an error message about an invalid DG class and a non-zero exit code. The cargo item
is not loaded; the program does not crash.

---

## Solution

<details>
<summary>Checkpoint answers (expand to check your work)</summary>

**1a**

- `id` is `char[32]` — up to 31 printable characters plus a null terminator.
- Weight is stored in kilograms; the file uses tonnes.
- `pos_x < 0` (specifically `−1.0`) means the item has not been placed by the optimizer.
- `Ship.cargo` is `Cargo *` — a pointer to a heap-allocated array of `Cargo` structs.
- The three optional sub-structs are `hydro`, `tanks`, and `strength_limits`.

**3**

`ContainerA.dimensions[0] = 12.2`, `[1] = 2.4`, `[2] = 2.6` (L × W × H in metres).

**5**

`safe_atof` rejects `0` because the minimum is `0.1f`. The parser returns `−1`. The CLI
prints a clean error; exit code is non-zero; no crash, no undefined behaviour.

**6**

The `Hydrostatics:` line changes from `box-hull fallback` to `table (N rows)` because
`Ship.hydro` is now a non-NULL pointer to a populated `HydroTable`.

**7**

`dg_class = 5`, `dg_division = 1`.

</details>

---

## Recap

- `Cargo` holds id, weight (kg), dimensions (m), type, position (m, or −1.0 unplaced),
  and an optional heap-allocated `DGInfo` pointer for dangerous goods.
- `Ship` holds vessel dimensions, the lightship weight and KG, a heap-allocated `cargo`
  array, and three optional sub-structs (`hydro`, `tanks`, `strength_limits`) that are
  `NULL` when not loaded.
- Config keys map 1-to-1 to struct fields via `parse_ship_config`; weight keys are
  multiplied by 1 000 inside the parser to convert tonnes → kilograms.
- `safe_atof` enforces numeric range at parse time, so invalid input exits cleanly instead
  of propagating garbage values into the physics.
- A `NULL` pointer on an optional field (like `Ship.hydro`) is a deliberate design choice:
  it signals "use the fallback" and is checked by every consumer before dereferencing.

*Next: [Pointers and addresses](09-pointers-and-addresses.md).*
