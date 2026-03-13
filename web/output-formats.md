# Output Formats

## Human (default)

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

Stability
  KG / KB / BM          : 2.26 / 0.64 / 48.62 m
  GM (transverse)       : 47.00 m

IMO Intact Stability (MSC.267/85)
  Overall               : COMPLIANT
```

## JSON

```bash
./cargoforge optimize ship.cfg cargo.txt --format=json
```

Structured JSON with `ship`, `cargo[]`, and `analysis` sections including hydrostatics, trim/heel, IMO stability, free surface correction, and longitudinal strength data. Pipe to `jq`:

```bash
./cargoforge optimize ship.cfg cargo.txt --format=json -q | jq '.analysis.imo_stability'

# Extract corrected GM and strength data
./cargoforge optimize ship.cfg cargo.txt --format=json -q | \
  jq '{gm: .analysis.gm_corrected, strength: .analysis.longitudinal_strength}'
```

## CSV

```bash
./cargoforge optimize ship.cfg cargo.txt --format=csv > results.csv
```

One row per cargo item. Columns: ID, Weight, Dimensions, Type, Placed, Position.

## Table

```bash
./cargoforge optimize ship.cfg cargo.txt --format=table
```

ASCII table with alignment. Supports `--only-placed`, `--only-failed`, and `--type` filters.

## Markdown

```bash
./cargoforge optimize ship.cfg cargo.txt --format=markdown > report.md
```

Markdown report with ship specs, cargo placement table, stability summary, and (when configured) free surface correction and longitudinal strength sections.
