# Config and manifest formats

CargoForge-C reads two plain-text files before any calculation happens: a ship configuration file
that describes the vessel, and a cargo manifest that lists what is being loaded. Understanding
these formats lets you feed real data to the program, interpret parse errors, and extend a
configuration with hydrostatic tables, tank definitions, or structural limits — capabilities that
unlock the full physics engine.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Ship config file (`key=value`)** = "a plain text recipe card that tells CargoForge-C what the vessel is" — five mandatory keys give `parse_ship_config` the dimensions and lightship weight it stores in the `Ship` struct before any cargo is touched.
- **Cargo manifest (whitespace-delimited columns)** = "a row-per-item list of what you're loading, how heavy it is, how big, and what kind" — `parse_cargo_list` turns each line into a `Cargo` entry with weight already converted to kilograms and dimensions split into the `dimensions[3]` array.
- **Tonne-to-kilogram conversion at parse time** = "the files speak tonnes because ship people always do; the code speaks kilograms; the parser is the translator" — every `weight_tonnes` value is multiplied by 1000 on entry so that `perform_analysis` and every stability formula inside it never has to think about units again.
- **`safe_atof` with range checks** = "a bounce-back guard on every number field" — it calls `strtof`, rejects anything outside the valid range, and returns `NAN` so the caller (`parse_ship_config` or `parse_cargo_list`) can bail cleanly with `-1` instead of silently storing a zero or garbage value.
- **`hydrostatic_table` path in the config** = "a pointer to a measured data file that replaces the built-in box-hull guesses" — when loaded, `perform_analysis` switches from the fixed $C_B = 0.75$ and $C_W = 0.85$ approximations to linear interpolation of real KB, BM, and MTC values at the actual draft.
- **`tank_config` and free-surface correction** = "a list of partially-filled tanks whose sloshing liquid makes the ship wobblier than its weight alone suggests" — `calculate_virtual_kg_rise` sums each tank's free-surface moment and subtracts the result from GM, giving `GM_corrected`; skipping this file leaves `GM` unconservatively high.
- **Freeing and NULLing on parse error** = "when something in the cargo list is wrong, every item already allocated is freed and the pointers are set to NULL immediately" — this is the direct lesson from the use-after-free bug recorded in the journal: `parse_cargo_list` sets `ship->cargo = NULL` after freeing so `ship_cleanup` cannot reach a dangling pointer.

**Why it matters:** if either file has a typo or a missing field, the program rejects the entire load cleanly rather than computing stability with corrupted data — but only because `safe_atof`, the two-phase error cleanup, and the NULL-after-free discipline are all working together. Get any one of those wrong and you get silent bad results or a crash, not an error message.

---

## The mental model 🧠

You'll forget the exact column order — hold THIS picture instead:

> Think of the two files as a *passport* and a *boarding list*. The ship config (`key=value`) is the vessel's passport: a small card of fixed facts — length, width, max weight, lightship KG — checked once at the gate by `parse_ship_config` before anyone boards. The cargo manifest is the boarding list: one row per item, read line-by-line by `parse_cargo_list`, stamping each `Cargo` entry into the `ship->cargo[]` array like a seat assignment.

Both documents speak **tonnes** because seafarers always do, but the gate agent (`safe_atof`) converts every figure to kilograms on the spot — so the physics engine (`perform_analysis`) and every stability formula it calls sees one consistent unit from the very first field. If a row on the boarding list is malformed, the gate doesn't try to salvage the rest: it tears up the entire list, frees every allocated `Cargo` slot, and NULLs the pointer so `ship_cleanup` cannot reach a dangling address. The extended keys (`hydrostatic_table`, `tank_config`) are optional attachments stapled to the passport — without them the gate uses conservative box-hull estimates; with them it switches to ship-specific measured data.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Config and manifest parse flow</title>
  <desc>Two input files — sample_ship.cfg and sample_cargo.txt — feed into parse_ship_config and parse_cargo_list respectively via safe_atof, which validates and converts tonnes to kg. Both populate fields of the Ship struct that perform_analysis reads.</desc>

  <!-- ship config file box -->
  <rect x="10" y="30" width="150" height="72" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="85" y="52" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">sample_ship.cfg</text>
  <text x="85" y="68" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">key=value</text>
  <text x="85" y="84" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">length_m, width_m</text>
  <text x="85" y="98" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">lightship_kg_m …</text>

  <!-- cargo manifest file box -->
  <rect x="10" y="200" width="150" height="72" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="85" y="222" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">sample_cargo.txt</text>
  <text x="85" y="238" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">whitespace columns</text>
  <text x="85" y="254" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">ID weight dims type</text>
  <text x="85" y="270" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">[DG field]</text>

  <!-- safe_atof box (shared) -->
  <rect x="215" y="118" width="130" height="44" rx="6" fill="none" stroke="#12A594" stroke-width="2"/>
  <text x="280" y="136" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">safe_atof</text>
  <text x="280" y="152" text-anchor="middle" font-size="10" fill="#12A594">validate + t → kg</text>

  <!-- arrow: cfg -> safe_atof -->
  <line x1="160" y1="66" x2="215" y2="130" stroke="currentColor" stroke-width="1.4" opacity="0.6" marker-end="url(#arr)"/>
  <!-- arrow: cargo -> safe_atof -->
  <line x1="160" y1="236" x2="215" y2="152" stroke="currentColor" stroke-width="1.4" opacity="0.6" marker-end="url(#arr)"/>

  <!-- parse_ship_config box -->
  <rect x="215" y="20" width="150" height="44" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.8"/>
  <text x="290" y="38" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">parse_ship_config</text>
  <text x="290" y="54" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.7">reads .cfg, calls safe_atof</text>

  <!-- parse_cargo_list box -->
  <rect x="215" y="240" width="150" height="44" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.8"/>
  <text x="290" y="258" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">parse_cargo_list</text>
  <text x="290" y="274" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.7">rows → Cargo[], NULL on err</text>

  <!-- arrows parse fns <-> safe_atof -->
  <line x1="290" y1="64" x2="290" y2="118" stroke="currentColor" stroke-width="1.2" opacity="0.5" stroke-dasharray="4,3"/>
  <line x1="290" y1="240" x2="290" y2="162" stroke="currentColor" stroke-width="1.2" opacity="0.5" stroke-dasharray="4,3"/>

  <!-- Ship struct box -->
  <rect x="430" y="80" width="170" height="144" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.8"/>
  <text x="515" y="100" text-anchor="middle" font-size="12" font-weight="700" fill="currentColor">Ship struct</text>
  <text x="515" y="118" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">length, width, max_weight</text>
  <text x="515" y="133" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">lightship_weight, lightship_kg</text>
  <text x="515" y="148" text-anchor="middle" font-size="10" fill="#12A594">cargo[0..n-1]  (kg)</text>
  <text x="515" y="163" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.6">hydro (optional)</text>
  <text x="515" y="178" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.6">tanks (optional)</text>
  <text x="515" y="193" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.6">strength_limits (optional)</text>
  <text x="515" y="210" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.5">→ perform_analysis()</text>

  <!-- arrows parse fns -> Ship struct -->
  <line x1="365" y1="42" x2="430" y2="120" stroke="currentColor" stroke-width="1.4" opacity="0.65" marker-end="url(#arr)"/>
  <line x1="365" y1="262" x2="430" y2="185" stroke="currentColor" stroke-width="1.4" opacity="0.65" marker-end="url(#arr)"/>

  <!-- error path label -->
  <text x="215" y="295" font-size="10" fill="#D05663" opacity="0.9">parse error → free + NULL → clean exit</text>

  <defs>
    <marker id="arr" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L8,3 z" fill="currentColor" opacity="0.65"/>
    </marker>
  </defs>
</svg>

## The ship configuration file

Ship config files use a minimal `key=value` format. Comment lines begin with `#`; blank lines are
ignored. There is no section syntax and no quoting — every value is either a number or a file
path.

The basic example, [`examples/sample_ship.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship.cfg), covers the five keys that are always required:

```ini
# Ship Specifications
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data for stability calculations
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

### What each key means

| Key | Unit stored | Converted to |
|---|---|---|
| `length_m` | metres | stored as-is (`ship->length`) |
| `width_m` | metres | stored as-is (`ship->width`) |
| `max_weight_tonnes` | tonnes (input) | kilograms (`× 1000`, stored in `ship->max_weight`) |
| `lightship_weight_tonnes` | tonnes (input) | kilograms (`× 1000`, stored in `ship->lightship_weight`) |
| `lightship_kg_m` | metres | stored as-is (`ship->lightship_kg`) |

The tonne-to-kilogram conversion matters: every weight calculation inside CargoForge-C works in
kilograms, but ship documents universally use tonnes. The parser does the conversion so the rest
of the code never has to.

!!! note "Why `lightship_kg_m`?"
    The lightship (the vessel without cargo or ballast) has its own vertical centre of gravity,
    called $KG_{light}$. When cargo is added, the overall KG is the weighted average of the
    lightship moment and the cargo moments. Supplying `lightship_kg_m` lets the program compute
    $KG$ correctly from the very first item loaded — omitting it would make every stability result
    wrong.

### Numeric validation

All numeric keys pass through `safe_atof` in `parser.c`, which calls `strtof`, checks the result
against the range $[0.1,\;10^9]$, and returns `NAN` on failure. If any mandatory key yields
`NAN`, `parse_ship_config` returns `-1` and nothing is loaded. This is why a typo such as
`length_m=abc` produces a clear error rather than a silent zero or a crash.

---

## The cargo manifest

The cargo manifest uses whitespace-delimited columns, not `key=value`. Each data line has four
mandatory fields and one optional field:

```
<ID>  <weight_tonnes>  <LxWxH_m>  <type>  [DG_field]
```

From [`examples/sample_cargo.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_cargo.txt):

```text
# Cargo Manifest
# ID              Weight(t) Dimensions(LxWxH)   Type
HeavyMachinery    550       20x5x3              standard
SteelBeams        400       18x2x2              bulk
ContainerA        250       12.2x2.4x2.6        reefer
ContainerB        250       12.2x2.4x2.6        reefer
SmallCrate        50        2x2x2               general
```

Lines starting with `#` are skipped; blank lines are skipped.

### Column-by-column

**ID** (`Cargo.id`, `char[32]`): A user-assigned identifier. The parser copies up to 31 bytes —
longer strings are silently truncated. The ID appears in all output formats and in IMDG violation
reports, so make it meaningful and short.

**Weight** (`Cargo.weight`, stored as kg): Parsed by `safe_atof` with range $[0.1,\;10^6]$ in
tonnes, then multiplied by 1000. `SteelBeams 400` → 400 000 kg.

**Dimensions** (`Cargo.dimensions[3]`): Three positive numbers joined by `x` with no spaces —
`LxWxH` in metres. The parser splits on `x` and validates each component individually. All three
must be present; `20x5` alone is a parse error.

**Type** (`Cargo.type`, `char[16]`): One of `standard`, `bulk`, `reefer`, `hazardous`,
`fragile`, or `general`. The type string governs constraint checks during bin-packing: reefer
cargo receives an advisory to stay on deck; fragile cargo has a tighter stacking-pressure limit;
hazardous cargo triggers the legacy 3 m separation check when no DG field is present.

**DG field** (optional, `Cargo.dg`): When a fifth token is present it is handed to
`parse_dg_field`. The grammar is:

```
DG:<class>[.<division>]:<UNnumber>:<stowage>:<EmS>
```

For example:

```text
FlammableLiquid  25  6x2.5x2.6  hazardous  DG:3.1:UN1203:A:F-E,S-D
```

`3.1` means IMDG Class 3, Division 1. `UN1203` is the UN number for petrol. `A` means the cargo
may be stowed anywhere. `F-E,S-D` is the EmS reference. When this field is present, the full
IMDG segregation engine (not just the legacy 3 m check) governs placement.

!!! warning "Parse errors clean up completely"
    If `safe_atof` rejects a weight field or a dimension component is missing, `parse_cargo_list`
    frees every `Cargo` item it has already allocated, sets `ship->cargo = NULL`, and sets
    `ship->cargo_count = 0`. This prevents a dangling pointer from reaching `ship_cleanup`. The
    lesson is recorded in the bug journal: freeing a pointer without NULLing it causes
    heap-use-after-free on the next access.

### Two-pass parsing for files, single-pass for stdin

When reading from a file, `parse_cargo_list` makes two passes: the first counts valid data lines,
the second populates the allocated array. When reading from stdin (filename `"-"`), only one pass
is possible, so the parser uses a temporary buffer. This distinction matters for memory
efficiency: the two-pass approach allocates exactly the right number of `Cargo` slots, whereas
the stdin path may over-allocate.

---

## The extended ship config

The five basic keys are sufficient for box-hull calculations. To activate table-based
hydrostatics, free-surface correction, and longitudinal strength checking, add the extended keys
shown in [`examples/sample_ship_full.cfg`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_ship_full.cfg):

```ini
# Full ship configuration with hydrostatic table, tanks, and strength limits
# MV Example - 150m x 25m general cargo vessel

# Ship dimensions
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data
lightship_weight_tonnes=2000
lightship_kg_m=8.0

# Hydrostatic table (enables table-based calculations)
hydrostatic_table=examples/sample_hydro.csv

# Tank configuration (enables free surface correction)
tank_config=examples/sample_tanks.csv

# Permissible longitudinal strength limits (from class society)
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

### `hydrostatic_table` — replacing the box-hull approximation

This key takes a file path. When present, `parse_ship_config` calls `parse_hydro_table` on that
path. A successful load sets `ship->hydro->loaded = 1`, which tells `perform_analysis` to use
table-based interpolation instead of the box-hull formulas.

The CSV format is comma-separated with `#` comments allowed. Each row represents one waterline:

```
draft_m, displacement_t, km_m, kb_m, bm_m, tpc_t_per_cm, mtc_tm_per_cm [, waterplane_area_m2 [, lcb_m]]
```

Rows must be in **strictly ascending draft order**; at least two rows are required. The seven-
column form is the minimum; columns 8 and 9 (`waterplane_area_m2`, `lcb_m`) default to zero if
absent. At runtime, `hydro_draft_from_displacement` inverse-interpolates draft from the computed
displacement, and `hydro_interpolate` then reads KB, BM, MTC, and KM at that draft — all by
linear interpolation between the two nearest rows.

Without this file, CargoForge-C falls back to:

$$
\text{draft} = \frac{\Delta_\text{vol}}{L \times B \times C_B}, \quad C_B = 0.75
$$

$$
KB = 0.53 \times \text{draft}, \qquad BM = \frac{L \times B^3 \times C_W}{12 \times \Delta_\text{vol}}, \quad C_W = 0.85
$$

The table replaces all three of these constants with ship-specific measured values — essential for
any result you would stake cargo on.

### `tank_config` — enabling free-surface correction

This key points to a tank CSV. When loaded, it activates `calculate_virtual_kg_rise`, which adds
a virtual rise in KG for each partially-filled tank:

$$
\text{FSM}_\text{tank} = \rho \cdot \frac{l \cdot b^3}{12}
$$

$$
\text{FSC} = \frac{\sum \text{FSM}_i}{\Delta_t}, \qquad GM_\text{corrected} = GM - \text{FSC}
$$

The tank CSV format is:

```
id, length_m, breadth_m, height_m, pos_x_m, pos_y_m, pos_z_m, fill_fraction, density_t_per_m3
```

All nine fields are required. `fill_fraction` is clamped to $[0, 1]$; tanks that are completely
empty or completely full contribute zero FSM (they have no free surface). A typical seawater
ballast tank uses `density=1.025`; fuel oil is typically `0.85`.

Without this file, `ship->tanks` is `NULL` and `perform_analysis` skips the free-surface
correction — `GM_corrected` equals `GM`, which is unconservative when ballast tanks are slack.

### Longitudinal strength keys

Three optional keys tell the program the limits issued by the classification society:

| Key | Stored in | Meaning |
|---|---|---|
| `permissible_sf_tonnes` | `StrengthLimits.permissible_sf` | Maximum still-water shear force (t) |
| `permissible_bm_hog_t_m` | `StrengthLimits.permissible_bm_hog` | Maximum hogging bending moment (t·m) |
| `permissible_bm_sag_t_m` | `StrengthLimits.permissible_bm_sag` | Maximum sagging bending moment (t·m) |

All three must be present for strength checking to activate. If any one is absent, `ship->strength_limits` remains `NULL` and `AnalysisResult.strength_compliant` is set to `-1` (meaning "not checked").

`calculate_longitudinal_strength` distributes weight and buoyancy across 20 hull stations, integrates to shear force and then to bending moment, and compares the peaks against these limits via `check_strength_limits`.

---

## How the parser resolves file paths

`parse_ship_config` stores the string value of `hydrostatic_table` or `tank_config` into a path
buffer, then calls the corresponding parser on that path. Paths are resolved relative to the
working directory from which CargoForge-C is invoked — not relative to the config file itself.
This is why the examples use paths like [`examples/sample_hydro.csv`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_hydro.csv) when the program is expected
to be run from the repository root.

!!! tip "Reading from stdin"
    Both `parse_ship_config` and `parse_cargo_list` accept `"-"` as the filename, which opens
    `stdin` instead of a file. This enables pipeline use:
    ```sh
    cat my_ship.cfg | cargoforge optimize - cargo.txt
    ```
    The two-pass strategy for counting lines is not possible on stdin, so the cargo parser uses
    a single-pass approach with a pre-allocated buffer in that case.

---

## Putting it together — what the parser feeds the physics engine

After both files parse successfully, the `Ship` struct holds:

- `ship->length`, `ship->width`, `ship->max_weight`, `ship->lightship_weight`, `ship->lightship_kg`
- `ship->cargo[0..cargo_count-1]` — each with weight (kg), dimensions (m), type string, position
  set to `-1.0` (unplaced), and optionally a heap-allocated `DGInfo`
- `ship->hydro` — `NULL` (box-hull) or a loaded `HydroTable`
- `ship->tanks` — `NULL` (no correction) or a loaded `TankConfig`
- `ship->strength_limits` — `NULL` (not checked) or loaded limits

`perform_analysis` reads exactly these fields. No other source of truth exists inside the library.

---

## Recap

- Ship config is `key=value`; five keys are mandatory, three optional groups (hydrostatics, tanks,
  strength limits) unlock increasingly precise physics.
- All weight keys are in **tonnes** on disk; the parser converts them to kilograms before storing.
- Cargo manifests are whitespace-delimited with four mandatory columns and an optional DG field
  using the grammar `DG:<class>[.div]:<UN>:<stowage>:<EmS>`.
- The `hydrostatic_table` path replaces three box-hull constants with measured, interpolated
  values; `tank_config` enables free-surface correction; the strength keys enable SWSF/SWBM
  checking.
- Parse errors clean up completely: freed arrays are NULLed immediately to prevent
  heap-use-after-free in `ship_cleanup`.

*Next: [Tokenizing and parsing in C](27-tokenizing-and-parsing-in-c.md).*
