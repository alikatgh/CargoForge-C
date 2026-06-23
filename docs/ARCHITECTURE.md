# Architecture

CargoForge-C is a small batch pipeline: read two files, decide where the cargo
goes, compute whether the result floats upright, print the plan. This document
explains the modules, the data that flows between them, and the math.

## Pipeline

```
            parse_ship_config ─┐
argv ──▶ main ──▶              ├─▶ optimize_cargo_placement ──▶ perform_analysis ──▶ print_loading_plan[_json]
            parse_cargo_list ──┘        (placement_2d)              (analysis)            (analysis)
```

1. **Parse** the ship config and cargo manifest into a `Ship` (`parser.c`).
2. **Optimize** placement — assign every `Cargo` a position (`optimizer.c` →
   `placement_2d.c`).
3. **Analyze** — compute center of gravity and metacentric height (`analysis.c`).
4. **Report** — human-readable table or `--json` (`analysis.c`).

`main.c` owns argument parsing (`--help`, `--version`, `--json`, two file paths)
and the single heap allocation's lifetime (`ship.cargo`).

## Modules

| File | Responsibility |
|------|----------------|
| `main.c` | CLI parsing, pipeline orchestration, memory cleanup |
| `parser.c` | `key=value` ship config + whitespace cargo manifest → `Ship`; validation |
| `optimizer.c` | Thin coordinator; delegates to the placement module |
| `placement_2d.c` | 2D shelf bin-packer, CG-aware bin selection, transverse trim |
| `analysis.c` | CG + GM stability math; text and JSON reporting |
| `cargoforge.h` | Shared data structures and the public API |

## Data structures (`cargoforge.h`)

- **`Ship`** — hull dimensions, weight limits, lightship data, and a heap array of
  `Cargo` (`cargo`, `cargo_count`, `cargo_capacity`).
- **`Cargo`** — id, weight, `dimensions[3]` (L×W×H), and the result fields
  `pos_x/y/z` plus `placed_w/placed_h` (footprint after rotation).
- **`Bin`** — a stowage region (a hold or the deck): origin offset, extents `w`×`h`,
  and a list of `Shelf` rows.
- **`Shelf`** — one packed row within a bin: its `y`, `height`, and `used_width`.
- **`AnalysisResult`** — CG (`perc_x`, `perc_y`), `gm`, placed weight and count.

**Units convention:** weights are stored internally in **kilograms** and distances
in **metres**. The config files use tonnes; `parser.c` converts at the boundary.

**Sentinel:** `pos_x < 0` means "not yet placed." `parser.c` initializes it
explicitly (malloc does not zero), and both placement and analysis rely on it.

## Placement algorithm (`placement_2d.c`)

1. **Sort** cargo heaviest-first (First-Fit-Decreasing; heavy items dominate
   stability and are hardest to place).
2. For each item, ask each bin — two holds (fore/aft, below deck) and the deck —
   for a candidate spot via `find_placement()`: it tries both rotations, prefers
   reusing an existing shelf over opening a new row, and bounds-checks **both** the
   X-extent (length) and Y-extent (beam). The decision is a `Placement` struct.
3. **Choose** the candidate bin whose resulting center of gravity is closest to
   amidships/centerline (`calculate_cg_for_placement`), then `commit_placement()`
   applies exactly that decision — one source of truth, so the finder and committer
   cannot drift.
4. **Transverse trim:** shelves stack from `y=0` upward, piling mass against one
   side of the beam. A final per-bin uniform Y-shift centers each bin's loaded band
   on the centerline. Because the shift is uniform within a bin it cannot create
   overlaps or push cargo out of bounds.

Coordinate convention: **X** runs along the ship's length, **Y** across the beam,
**Z** is the deck level (negative = in a hold, 0 = on deck).

## Stability math (`analysis.c`)

**Center of gravity** is the weight-weighted mean position of the lightship plus
all placed cargo, reported as a percentage of length (longitudinal) and beam
(transverse). ~50% / 50% is ideal.

**Metacentric height** is the righting-arm lever:

```
GM = KB + BM − KG
```

- **KG** — height of the combined center of gravity above the keel, from the
  lightship vertical moment plus each item's `pos_z + height/2` contribution.
- **KB** — center of buoyancy height ≈ draft / 2, where
  `draft = displaced_volume / (length × beam × waterplane_coefficient)`.
- **BM** — `I / V`: transverse waterplane moment of inertia over displaced volume
  (`I = length × beam³ / 12 × waterplane_coefficient`).

`GM > 0` means the ship self-rights when heeled; `GM ≤ 0` means it capsizes. A plan
whose total weight exceeds the ship's maximum is rejected outright (`GM = NaN`).

Constants: seawater density 1.025 t/m³, waterplane coefficient 0.8.

## Testing strategy

Tests assert **invariants** and **hand-computed values**, never frozen heuristic
output (see `tests/`):

- `test_parser` — validation, sentinel initialization, over-long-line handling.
- `test_placement_2d` — all placed, in bounds, no overlap, oversize rejected,
  transverse load centered.
- `test_analysis` — CG/GM for a known load, overweight rejection, top-heavy
  instability (negative GM).

`scripts/check.sh` runs the suite under gcc and clang with `-Werror`, the Clang
static analyzer, the example scenarios, and AddressSanitizer + UBSan — the same
gates CI enforces.
