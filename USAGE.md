# CargoForge-C Usage Guide

## Table of Contents

- [Quick Start](#quick-start)
- [Input File Formats](#input-file-formats)
  - [Ship Configuration](#ship-configuration-cfg)
  - [Cargo Manifest](#cargo-manifest-txt)
  - [Hydrostatic Table](#hydrostatic-table-csv)
  - [Tank Configuration](#tank-configuration-csv)
- [Subcommands](#subcommands)
- [Output Formats](#output-formats)
- [Configuration File](#configuration-file)
- [Understanding the Output](#understanding-the-output)
- [Examples](#examples)

---

## Quick Start

```bash
# Build
make

# Run with sample data (box-hull fallback)
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt

# Run with full hydrostatic tables, tanks, and strength limits
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt

# Run with dangerous goods cargo
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

You'll get a full stability analysis with cargo placement, trim/heel calculations, IMO compliance check, and (when configured) free surface correction and longitudinal strength verification.

---

## Input File Formats

### Ship Configuration (.cfg)

Key-value pairs defining the vessel. Lines starting with `#` are comments.

**Minimal configuration (box-hull fallback):**

```
# Ship Specifications
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data for stability calculations
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

**Full configuration (hydrostatic tables, tanks, strength limits):**

```
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0

# Hydrostatic table (enables real stability booklet data)
hydrostatic_table=examples/sample_hydro.csv

# Tank configuration (enables free surface correction)
tank_config=examples/sample_tanks.csv

# Permissible longitudinal strength limits (from class society)
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

| Field | Unit | Description | Required |
|-------|------|-------------|----------|
| `length_m` | meters | Length overall (LOA) | Yes |
| `width_m` | meters | Beam (breadth) | Yes |
| `max_weight_tonnes` | tonnes | Maximum displacement | Yes |
| `lightship_weight_tonnes` | tonnes | Empty ship weight | Yes |
| `lightship_kg_m` | meters | Lightship vertical CG above keel | Yes |
| `hydrostatic_table` | path | Path to hydrostatic CSV file | No |
| `tank_config` | path | Path to tank configuration CSV file | No |
| `permissible_sf_tonnes` | tonnes | Max allowable shear force | No |
| `permissible_bm_hog_t_m` | t-m | Max allowable hogging bending moment | No |
| `permissible_bm_sag_t_m` | t-m | Max allowable sagging bending moment | No |

All optional fields enable their respective features when present. When absent, the analysis falls back to simpler models (box-hull hydrostatics, no free surface correction, no strength check).

### Cargo Manifest (.txt)

Space-separated columns. Lines starting with `#` are comments.

**Basic format:**

```
# ID              Weight(t)  Dimensions(LxWxH)  Type
HeavyMachinery    550        20x5x3             standard
SteelBeams        400        18x2x2             bulk
ContainerA        250        12.2x2.4x2.6       reefer
GlassPanel        30         4x3x1              fragile
```

**With dangerous goods (DG) information (optional 5th field):**

```
# ID              Weight(t)  Dimensions(LxWxH)  Type       DG info
FlammableLiquid   25         6x2.5x2.6          hazardous  DG:3.1:UN1203:A:F-E,S-D
OxidizerDrum      15         6x2.5x2.6          hazardous  DG:5.1:UN1942:A:F-A,S-Q
CorrosiveAcid     20         6x2.5x2.6          hazardous  DG:8.0:UN1789:A:F-A,S-B
```

| Column | Format | Description |
|--------|--------|-------------|
| ID | string | Unique identifier (max 31 chars) |
| Weight | float | Weight in tonnes |
| Dimensions | `LxWxH` | Length x Width x Height in meters |
| Type | string | Cargo classification (see below) |
| DG info | `DG:class.div:UN:stow:EmS` | Optional IMDG dangerous goods info |

**DG field format:** `DG:class.division:UNnumber:stowage:EmS`

| Part | Example | Description |
|------|---------|-------------|
| class.division | `3.1` | IMDG class and division |
| UNnumber | `UN1203` | UN number |
| stowage | `A` | `A` = any, `D` = deck only, `U` = under deck only |
| EmS | `F-E,S-D` | Emergency Schedule reference |

When DG info is present, the full IMDG Code Table 7.2.4 segregation matrix is used instead of the basic 3m separation rule.

**Cargo types:**

| Type | Constraints Applied |
|------|-------------------|
| `standard` | Point load, deck weight ratio |
| `hazardous` | IMDG segregation (full matrix if DG info present, 3m fallback otherwise) |
| `reefer` | Prefers deck placement (reefer plug access) |
| `fragile` | Lower stacking pressure limit (5 t/m2 vs 20 t/m2) |
| `heavy` | Point load, deck weight ratio |
| `bulk` | Standard constraints |

### Hydrostatic Table (.csv)

CSV file with real hydrostatic data from the vessel's stability booklet. Lines starting with `#` are comments. Entries must be in ascending draft order. At least 2 entries required.

```
# draft_m,displacement_t,KM_m,KB_m,BM_m,TPC_t_cm,MTC_t_m,waterplane_m2,LCB_m
2.0,2306,13.20,1.06,12.14,9.60,185.0,3375,1.5
3.0,3544,9.92,1.58,8.34,10.20,195.0,3570,1.2
4.0,4838,8.23,2.10,6.13,10.70,205.0,3720,0.9
5.0,6181,7.25,2.63,4.62,11.10,215.0,3840,0.6
6.0,7569,6.68,3.14,3.54,11.40,225.0,3930,0.3
```

| Column | Unit | Description |
|--------|------|-------------|
| draft_m | m | Even-keel draft |
| displacement_t | tonnes | Displacement at this draft |
| KM_m | m | Height of transverse metacentre above keel |
| KB_m | m | Centre of buoyancy above keel |
| BM_m | m | Transverse metacentric radius |
| TPC_t_cm | t/cm | Tonnes per cm immersion |
| MTC_t_m | t-m | Moment to change trim 1 cm |
| waterplane_m2 | m^2 | Waterplane area (optional, can be 0) |
| LCB_m | m | Longitudinal centre of buoyancy from midship (optional) |

The last two columns are optional. A 7-column format (without waterplane and LCB) is also accepted.

### Tank Configuration (.csv)

CSV file defining the vessel's liquid tanks for free surface correction. Lines starting with `#` are comments.

```
# id,length_m,breadth_m,height_m,pos_x_m,pos_y_m,pos_z_m,fill_fraction,density_t_m3
BallastFP,8.0,12.0,6.0,140.0,0.0,0.0,0.50,1.025
BallastDB_P,20.0,4.0,6.0,75.0,-10.0,0.0,0.75,1.025
FuelOil_1,10.0,6.0,5.0,30.0,-8.0,0.0,0.60,0.950
FreshWater,5.0,5.0,4.0,20.0,0.0,0.0,0.85,1.000
```

| Column | Unit | Description |
|--------|------|-------------|
| id | string | Tank identifier |
| length_m | m | Tank length along ship |
| breadth_m | m | Tank breadth athwartships |
| height_m | m | Tank height |
| pos_x_m | m | Longitudinal position from AP |
| pos_y_m | m | Transverse position from centreline |
| pos_z_m | m | Vertical position of tank bottom above keel |
| fill_fraction | 0-1 | Fill level (0.0 = empty, 1.0 = full) |
| density_t_m3 | t/m^3 | Liquid density (seawater=1.025, fuel oil=0.95, fresh water=1.0) |

Only partially filled tanks (0 < fill < 1) produce free surface effects. Full and empty tanks have no free surface moment.

---

## Subcommands

### optimize

Runs the 3D bin-packing algorithm and performs stability analysis.

```bash
./cargoforge optimize <ship.cfg> <cargo.txt> [options]
```

Options:
- `--format=FORMAT` — Output format: `human`, `json`, `csv`, `table`, `markdown`
- `--output=FILE` — Write output to file instead of stdout
- `--no-viz` — Disable ASCII visualization
- `--only-placed` — Show only successfully placed cargo
- `--only-failed` — Show only cargo that couldn't be placed
- `--type=TYPE` — Filter output by cargo type
- `-v, --verbose` — Verbose output
- `-q, --quiet` — Suppress status messages

### validate

Checks input files for errors without running optimization.

```bash
./cargoforge validate <ship.cfg> <cargo.txt>
```

Returns exit code 0 if valid, non-zero if errors found. Useful in scripts:

```bash
./cargoforge validate ship.cfg cargo.txt && ./cargoforge optimize ship.cfg cargo.txt
```

### info

Displays ship specifications and cargo statistics.

```bash
./cargoforge info <ship.cfg> [cargo.txt]
```

The cargo file is optional. If provided, shows cargo summary statistics.

### version

```bash
./cargoforge version
```

### help

```bash
./cargoforge help
./cargoforge help optimize
```

---

## Output Formats

### Human (default)

```
--- CargoForge Stability Analysis ---

Ship: 150.00 m x 25.00 m | Max Weight: 50000.00 t
----------------------------------------------------------------------
  - HeavyMachinery  | Pos (   0.00,    0.00,  -8.00) | 550.00 t
  - SteelBeams      | Pos (   0.00,    0.00,  -5.00) | 400.00 t

Load Summary
  Placed / Total        : 5 / 5
  Displacement          : 3500.00 t (7.0% of max)
  Draft                 : 1.21 m
  CG (Lon / Trans)      : 5.8% / 13.1% | Warning

Stability
  KG / KB / BM          : 2.26 / 0.64 / 48.62 m
  GM (transverse)       : 47.00 m | WARNING - Too stiff
  Trim (by stern)       : -2.436 m
  Heel                  : -11.10 deg

IMO Intact Stability (MSC.267/85)
  GZ at 30 deg          : 27.551 m  (min 0.200) OK
  Max GZ                : 816.281 m at 80 deg (min 25 deg) OK
  Area 0-30 deg         : 6.8004 m-rad (min 0.0550) OK
  Area 0-40 deg         : 12.7326 m-rad (min 0.0900) OK
  Area 30-40 deg        : 5.9320 m-rad (min 0.0300) OK
  Overall               : COMPLIANT
```

### JSON

```bash
./cargoforge optimize ship.cfg cargo.txt --format=json
```

Outputs structured JSON with `ship`, `cargo[]`, and `analysis` sections. The analysis includes full hydrostatics, trim/heel, and IMO stability data. Useful for piping to `jq` or consuming from other tools.

### CSV

```bash
./cargoforge optimize ship.cfg cargo.txt --format=csv > results.csv
```

One row per cargo item. Columns: ID, Weight, Dimensions, Type, Placed, Position.

### Table

```bash
./cargoforge optimize ship.cfg cargo.txt --format=table
```

ASCII table with alignment. Supports `--only-placed`, `--only-failed`, and `--type` filters.

### Markdown

```bash
./cargoforge optimize ship.cfg cargo.txt --format=markdown > report.md
```

Generates a markdown report with ship specs, cargo placement table, and stability summary.

---

## Configuration File

Create `~/.cargoforgerc` for global defaults or `.cargoforgerc` in your project directory.

```
# Output format (human, json, csv, table, markdown)
format=table

# Colored output (true/false)
color=yes

# Verbose mode
verbose=no

# Show ASCII visualization
show_viz=yes
```

Command-line flags override config file settings. Local config overrides global.

---

## Understanding the Output

### Hydrostatics Source

The output header indicates which hydrostatic model is used:

| Indicator | Meaning |
|-----------|---------|
| `[Hydrostatic table]` | Real stability booklet data interpolated from CSV |
| `[Box-hull model]` | Simplified coefficients (Cb=0.75) — no hydrostatic table loaded |

### Hydrostatics

| Parameter | Meaning |
|-----------|---------|
| **Draft** | How deep the ship sits in the water (meters) |
| **KG** | Vertical center of gravity above keel |
| **KB** | Vertical center of buoyancy above keel |
| **BM** | Metacentric radius (distance from B to M) |
| **GM** | Metacentric height = KB + BM - KG. Positive = stable |
| **GM (corrected)** | GM after free surface correction. Shown when tanks are loaded |
| **Free surface correction** | Virtual KG rise from partially filled tanks (meters) |

### GM Interpretation

| GM Range | Status | Meaning |
|----------|--------|---------|
| < 0.15 m | CRITICAL | Vessel may capsize. IMO non-compliant |
| 0.15 - 0.30 m | Tender | Stable but marginal |
| 0.30 - 0.50 m | Acceptable | Within safe range |
| 0.50 - 2.50 m | Optimal | Comfortable motion, safe |
| 2.50 - 3.00 m | Acceptable | Getting stiff |
| > 3.00 m | Too Stiff | Uncomfortable rolling, structural stress risk |

When tank data is loaded, the **corrected GM** (after free surface correction) is used for all stability assessments and IMO criteria.

### Trim

Positive trim = stern sits deeper than bow. Negative = bow deeper. Ideally close to zero or slightly by stern.

### Heel

Degrees of permanent list to one side. Should be < 1 degree for a well-loaded ship. Caused by asymmetric cargo placement.

### IMO Intact Stability Criteria

Based on MSC.267(85) Part A, Chapter 2.2. All six criteria must pass:

| Criterion | Minimum | What It Means |
|-----------|---------|---------------|
| GM >= 0.15 m | 0.15 m | Basic positive stability |
| GZ at 30 deg >= 0.20 m | 0.20 m | Righting arm at large heel |
| Max GZ angle >= 25 deg | 25 deg | Peak restoring force not too early |
| Area 0-30 deg | 0.055 m-rad | Energy to resist heeling to 30 deg |
| Area 0-40 deg | 0.090 m-rad | Energy to resist heeling to 40 deg |
| Area 30-40 deg | 0.030 m-rad | Reserve stability at large angles |

### Longitudinal Strength

Shown when permissible strength limits are configured in the ship config. Reports still water shear force (SWSF) and bending moment (SWBM) across 21 hull stations.

| Parameter | Meaning |
|-----------|---------|
| **Max SF** | Peak shear force along the hull (tonnes) |
| **Max BM (hog)** | Peak hogging bending moment (t-m) |
| **Max BM (sag)** | Peak sagging bending moment (t-m) |
| **SF ratio** | max_sf / permissible_sf (must be <= 1.0) |
| **BM ratio** | max_bm / permissible_bm (must be <= 1.0) |
| **Status** | WITHIN LIMITS or EXCEEDS LIMITS |

### IMDG Segregation

When cargo items carry DG (dangerous goods) information, the output includes an IMDG compliance section listing any segregation violations. Each violation shows the two cargo items, their DG classes, the required segregation level, and the actual distance between them.

---

## Examples

### Basic optimization
```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

### Full analysis with hydrostatic tables and tanks
```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt
```

### Dangerous goods cargo
```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

### Validate before running
```bash
./cargoforge validate ship.cfg cargo.txt && \
./cargoforge optimize ship.cfg cargo.txt
```

### JSON output piped to jq
```bash
./cargoforge optimize ship.cfg cargo.txt --format=json 2>/dev/null | jq '.analysis.imo_stability'
```

### Extract corrected GM and strength data
```bash
./cargoforge optimize ship.cfg cargo.txt --format=json -q 2>/dev/null | \
  jq '{gm: .analysis.gm_corrected, strength: .analysis.longitudinal_strength}'
```

### Save report as markdown
```bash
./cargoforge optimize ship.cfg cargo.txt --format=markdown --output=report.md
```

### Show only failed placements
```bash
./cargoforge optimize ship.cfg cargo.txt --format=table --only-failed
```

### Filter by cargo type
```bash
./cargoforge optimize ship.cfg cargo.txt --format=table --type=hazardous
```

### Quiet mode for scripting
```bash
./cargoforge optimize ship.cfg cargo.txt --format=json -q 2>/dev/null > results.json
```

### Read from stdin
```bash
cat cargo.txt | ./cargoforge optimize ship.cfg -
```
