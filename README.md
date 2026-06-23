# CargoForge-C

[![CI](https://github.com/alikatgh/CargoForge-C/actions/workflows/c-build.yml/badge.svg)](https://github.com/alikatgh/CargoForge-C/actions/workflows/c-build.yml)
![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Dependencies](https://img.shields.io/badge/dependencies-none-brightgreen.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A small, dependency-free **maritime cargo-stowage simulator** written in pure C99.
Give it a ship and a cargo manifest; it decides where each item goes, then reports
whether the resulting load is **stable enough to sail**.

It is a compact, readable model of two real problems naval architects solve every
day: **bin packing** (fit the cargo) and **ship stability** (don't capsize).

---

## What it computes

For a given ship and manifest, CargoForge-C produces a loading plan and two
stability figures that decide whether that plan is safe:

| Quantity | Meaning | Why it matters |
|----------|---------|----------------|
| **Center of Gravity (CG)** | Where the combined weight of ship + cargo acts, reported as % of length (longitudinal) and % of beam (transverse). | A CG far from amidships/centerline gives the ship a list or trim. ~50% / 50% is ideal. |
| **Metacentric Height (GM)** | `GM = KB + BM − KG`. The lever arm of the ship's righting moment. | **GM > 0 ⇒ the ship self-rights** when heeled. GM ≤ 0 ⇒ it capsizes. The single most important stability number. |

`GM` is built from the classic naval-architecture terms:

- **KB** — height of the center of buoyancy above the keel (≈ half the draft).
- **BM** — `I / V`, the transverse waterplane moment of inertia over the displaced
  volume (the ship's "form stability").
- **KG** — height of the center of gravity above the keel, from the lightship data
  plus every placed item's vertical moment.

An overweight plan (total weight beyond the ship's maximum) is rejected outright.

---

## Quickstart

```sh
make                 # build ./cargoforge (optimized)
make run             # build and run on the bundled sample scenario
make test            # build and run the unit-test suite
sudo make install    # install the binary + man page (PREFIX=/usr/local)
```

Run it directly:

```sh
./cargoforge examples/realistic_ship.cfg examples/realistic_cargo.txt
./cargoforge --json examples/realistic_ship.cfg examples/realistic_cargo.txt  # machine-readable
./cargoforge --strict examples/realistic_ship.cfg examples/realistic_cargo.txt  # non-zero exit on a bad plan
./cargoforge --quiet examples/sample_ship.cfg examples/sample_cargo.txt       # summary only
./cargoforge --verbose --color=always examples/sample_ship.cfg examples/sample_cargo.txt
./cargoforge --help        # usage and options
./cargoforge --version     # print version
```

The `--json` flag emits the plan and stability summary as a single JSON object
(ship specs, a `placements` array, and a `summary` with CG, GM, and `stable` /
`balanced` / `rejected` booleans) so the tool composes into scripts and pipelines.
The `--strict` flag makes the process exit non-zero when the plan isn't fully
successful — any cargo unplaced, overweight, or unstable (GM ≤ 0.15 m) — so CI and
automation can gate on it. Both flags may be combined and placed in any position.

### Example output

```
--- CargoForge Stability Analysis ---

Ship Specs: 150.00 m × 25.00 m | Max Weight: 20000.00 t
----------------------------------------------------------------------
  - HeavyEngine     | Pos (X,Y,Z): (  75.00,    0.00,   -5.00) | 60.00 t
  - SteelCoils_A    | Pos (X,Y,Z): (  75.00,    4.00,   -5.00) | 40.00 t
  - SteelCoils_B    | Pos (X,Y,Z): (  80.00,    4.00,   -5.00) | 40.00 t
  - BulkyGenerator  | Pos (X,Y,Z): (  75.00,    9.00,   -5.00) | 35.00 t
  ...
Load Summary
  - Placed / Total items : 10 / 10
  - Total loaded weight  : 295.00 t (26.5 % of max)
  - Displacement / DWT   : 5295.00 t / 295.00 t
  - CG (Lon / Trans)     : 50.0 % / 49.5 % | Balance: Good

Hydrostatics & Stability
  - Draft (mean)         : 1.72 m  (fore 1.72 / aft 1.72)
  - Displaced volume     : 5165.9 m³
  - KG / KB / BM         : 7.37 / 0.86 / 30.25 m
  - Trim / Heel          : -0.00 m / -0.3°
  - GM (transverse)      : 23.74 m | Stability: Stable
  - GML (longitudinal)   : 1082.37 m
```

`Pos (X,Y,Z)`: X is along the ship's length, Y across the beam, Z the deck level
(negative = below the main deck, in a hold; 0 = on deck).

---

## Input formats

### Ship config (`*.cfg`) — `key=value`, `#` comments

```ini
length_m=150
width_m=25
max_weight_tonnes=20000
lightship_weight_tonnes=5000   # empty-ship weight
lightship_kg_m=8               # empty-ship vertical CG above keel
holds=2                        # optional: number of below-deck holds (default 2)
depth_m=14                     # optional: moulded hull depth, enables freeboard
```

`length_m`, `width_m`, and `max_weight_tonnes` are **required**; a missing field
is rejected rather than silently defaulted to zero. `holds` is optional (1–50,
default 2) and sets how many below-deck holds split the length fore-to-aft.
Unknown keys are warned about and ignored.

### Cargo manifest (`*.txt`) — whitespace-separated, `#` comments

```
# ID            Weight(t)  Dimensions(LxWxH)  Type
HeavyEngine     60         10x4x5             breakbulk
StandardCont_1  25         12.2x2.4x2.6       container
```

Columns: identifier, weight in tonnes, `LxWxH` dimensions in metres, and a free-form
type label. Malformed lines are reported and skipped; invalid numbers abort the parse.

---

## How it works

1. **Sort** cargo heaviest-first (a First-Fit-Decreasing heuristic — heavy items
   are the hardest to place and dominate stability, so they go first).
2. For each item, evaluate three bins — two holds (fore/aft, below deck) and the
   deck — using a **2D shelf packer**: items fill a shelf along the ship's length,
   shelves stack across the beam, and both axes are bounds-checked.
3. Among the bins where the item fits, pick the placement that keeps the running
   **center of gravity closest to amidships/centerline** — stability-aware packing,
   not just "first spot that fits."
4. **Transverse trim:** center each bin's loaded band on the beam so mass isn't
   piled against one side (a uniform per-bin shift, so it never overlaps cargo or
   exceeds the hull).
5. Compute CG and GM and print the plan with a stability verdict.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the module/data-flow and
math details, and [`docs/BUG_JOURNAL.md`](docs/BUG_JOURNAL.md) for fixes and invariants.

---

## Project layout

```
cargoforge.h       shared data structures and the public API
main.c             CLI entry point and program flow
parser.c           ship-config and cargo-manifest parsing (+ validation)
optimizer.c        thin coordinator over the placement module
placement_2d.c     the 2D shelf bin-packer + CG-aware bin selection
analysis.c         CG and metacentric-height (GM) stability math + reporting
tests/             unit tests (parser, placement invariants, analysis math)
examples/          sample ship configs and cargo manifests
docs/              bug journal and notes
```

---

## Development

```sh
make WERROR=1 test   # build the suite with warnings promoted to errors (CI default)
make sanitize        # build ./cargoforge-san with AddressSanitizer + UBSan
make analyze         # run the Clang static analyzer
./scripts/fuzz.sh    # fuzz the parser under sanitizers (random adversarial input)
make debug           # -g -O0 build for a debugger
make format          # apply .clang-format (if clang-format is installed)
```

The CI pipeline ([`.github/workflows/c-build.yml`](.github/workflows/c-build.yml))
builds with **both gcc and clang** under `-Werror`, runs the full test suite, the
Clang static analyzer, and every example scenario under AddressSanitizer +
UndefinedBehaviorSanitizer.

The tests assert **invariants** (every item placed, in bounds, no overlap; CG/GM
match hand-computed values), not frozen output — so the heuristic can improve
without churning the suite.

---

## Limitations & roadmap

CargoForge-C is a teaching-grade model, not a classed stability program. Known
simplifications, roughly in priority order:

- **Longitudinal packing is greedy within a bin** — the transverse band is centered
  (step 4 above), but items still fill each shelf left-to-right, so the longitudinal
  distribution isn't optimized beyond the heaviest-first bin choice.
- **2D footprints only** — items are not stacked vertically within a hold; the
  third dimension feeds the GM math but not the packing.
- **Fixed bins** — two holds + deck, split at midships. Configurable compartments
  are a natural extension.
- **Static stability only** — no free-surface effect, no longitudinal GML/trim, no
  wind heeling.

Contributions welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

Released under the [MIT License](LICENSE).
