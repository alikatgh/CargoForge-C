# CargoForge-C Usage Guide

## Table of Contents

- [Quick Start](#quick-start)
- [Input File Formats](#input-file-formats)
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

# Run with sample data
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

That's it. You'll get a full stability analysis with cargo placement, trim/heel calculations, and IMO compliance check.

---

## Input File Formats

### Ship Configuration (.cfg)

Key-value pairs defining the vessel. Lines starting with `#` are comments.

```
# Ship Specifications
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data for stability calculations
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

| Field | Unit | Description |
|-------|------|-------------|
| `length_m` | meters | Length overall (LOA) |
| `width_m` | meters | Beam (breadth) |
| `max_weight_tonnes` | tonnes | Maximum displacement (deadweight + lightship) |
| `lightship_weight_tonnes` | tonnes | Empty ship weight |
| `lightship_kg_m` | meters | Lightship vertical center of gravity above keel |

### Cargo Manifest (.txt)

Space-separated columns. Lines starting with `#` are comments.

```
# ID              Weight(t)  Dimensions(LxWxH)  Type
HeavyMachinery    550        20x5x3             standard
SteelBeams        400        18x2x2             bulk
ContainerA        250        12.2x2.4x2.6       reefer
HazChem           100        3x2x2              hazardous
GlassPanel        30         4x3x1              fragile
```

| Column | Format | Description |
|--------|--------|-------------|
| ID | string | Unique identifier (max 31 chars) |
| Weight | float | Weight in tonnes |
| Dimensions | `LxWxH` | Length x Width x Height in meters |
| Type | string | Cargo classification (see below) |

**Cargo types:**

| Type | Constraints Applied |
|------|-------------------|
| `standard` | Point load, deck weight ratio |
| `hazardous` | 3m minimum separation from other hazmat (IMDG) |
| `reefer` | Prefers deck placement (reefer plug access) |
| `fragile` | Lower stacking pressure limit (5 t/m2 vs 20 t/m2) |
| `heavy` | Point load, deck weight ratio |
| `bulk` | Standard constraints |

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

### Hydrostatics

| Parameter | Meaning |
|-----------|---------|
| **Draft** | How deep the ship sits in the water (meters) |
| **KG** | Vertical center of gravity above keel |
| **KB** | Vertical center of buoyancy above keel |
| **BM** | Metacentric radius (distance from B to M) |
| **GM** | Metacentric height = KB + BM - KG. Positive = stable |

### GM Interpretation

| GM Range | Status | Meaning |
|----------|--------|---------|
| < 0.15 m | CRITICAL | Vessel may capsize. IMO non-compliant |
| 0.15 - 0.30 m | Tender | Stable but marginal |
| 0.30 - 0.50 m | Acceptable | Within safe range |
| 0.50 - 2.50 m | Optimal | Comfortable motion, safe |
| 2.50 - 3.00 m | Acceptable | Getting stiff |
| > 3.00 m | Too Stiff | Uncomfortable rolling, structural stress risk |

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

---

## Examples

### Basic optimization
```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
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
