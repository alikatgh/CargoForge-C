# Changelog

All notable changes to CargoForge-C are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Cargo logistics: optional `temp=`, `maxstack=`, and `dest=` manifest attributes,
  reported as a Cargo Logistics section — destination grouping, temperature zones,
  max-stack ratings, and a port-sequencing (LIFO) restow check.
- CSV cargo manifests (`*.csv`, header row auto-skipped; dimensions stay one
  `LxWxH` cell) and flat JSON ship configs (`*.json`), auto-detected by extension.
- `--units=metric|imperial` to display lengths in feet, weights in long tons, and
  volumes in cubic feet (metric remains the default and the JSON canonical units).
- `--sort=none|weight|id|position` to order the per-item placement list.
- Engine as a static library: `make lib` builds `libcargoforge.a` against the
  public `cargoforge.h` API, so the placement/analysis engine can be embedded.
- `make bench` (throughput benchmark on a synthetic large manifest) and
  `make coverage` (per-file line coverage via gcov).
- More output modes: `--summary` (one-line status), `--table` (box-drawing
  placements table), `--progress` (stage messages on stderr), and a consolidated
  "⚠ Warnings" recap at the end of the report.
- Per-hold weight cap via the optional `max_hold_weight_tonnes` config key (the
  deck is exempt as overflow); stowage-area-used and cargo-volume metrics; and an
  Unplaced-Cargo report listing what didn't fit (flagging priority items).
- Cargo attributes via an optional comma-separated 5th manifest field
  (`priority`, `reefer`, `fragile`, `nostack`, `oog`, `dg=N`). Drives a new
  "Cargo Notes" report: reefer power demand, lashing estimate, dangerous-goods
  classes + hazmat segregation warnings, and priority-unplaced alerts. Plain
  manifests are unaffected.
- Stability criteria & compliance: free-surface correction (for liquid/tank/ballast
  cargo) and corrected GM(fluid), righting arm GZ at 30°, GM safety margin, a
  simplified load-line/Plimsoll check, beam-wind heel, deck-edge immersion angle, a
  simplified IMO intact-stability PASS/FAIL, and a ballast suggestion when a plan
  fails or lists. Shown in the report and `--json`.
- Input flexibility: read any file from stdin with `-`; `--init` prints a sample
  config; `--show-config` echoes the parsed config; `CARGOFORGE_HOLDS` /
  `CARGOFORGE_DEPTH_M` environment overrides. Config now also accepts inline
  `# comments` and surrounding whitespace.
- Cargo validation & reporting: duplicate-ID detection, implausible-density
  warnings, and a weight-by-type summary (in `--verbose` and `--md`).
- `--csv` (RFC-4180 placements) and `--md` (Markdown report with a placements
  table and stability summary, including average/heaviest/lightest cargo).
- `--diagram`: an ASCII top-down stowage plan (holds + deck, cargo footprints),
  per-hold utilization bars, and a GM stability gauge.
- Colored terminal output with `--color=auto|always|never` (and `--no-color`),
  auto-detecting a TTY and honoring the `NO_COLOR` convention.
- `--quiet` (summary only) and `--verbose` (per-item CG contribution) output modes.
- Full hydrostatics report: vertical CG (KG), centre of buoyancy (KB), metacentric
  radius (BM), mean/fore/aft draft, trim, static heel, longitudinal metacentric
  height (GML), displacement, deadweight, displaced volume, and freeboard (when the
  optional `depth_m` ship-config key is given). Shown in the text report and `--json`.
- Optional `depth_m` ship-config key (moulded hull depth) enabling freeboard.
- Configurable number of below-deck holds via the optional `holds=` ship-config
  key (1–50, default 2). Holds split the ship's length evenly fore-to-aft.
- `--strict` flag: exit non-zero when a plan isn't fully successful (any cargo
  unplaced, overweight, or unstable), for use in CI and automation.
- Man page (`docs/cargoforge.1`) and `make install` / `make uninstall` targets
  (PREFIX/DESTDIR aware).
- `scripts/fuzz.sh`: a seeded random-input fuzzer that runs the sanitized binary
  against malformed configs/manifests and fails on any crash or sanitizer error;
  wired into `scripts/check.sh` and CI.

## [1.0.0] - 2026-06-23

The first hardened release: correct, tested, sanitizer-clean, and documented.

### Added
- `--help` / `-h` and `--version` / `-v` flags.
- `--json` output mode emitting the loading plan and stability summary as a single
  JSON object (ship specs, a `placements` array, and a `summary` with CG, GM, and
  `stable` / `balanced` / `rejected` booleans) for scripting and pipelines.
- Transverse-trim pass that centers each bin's loaded band on the beam, improving
  the transverse center of gravity (sample scenario 31% → 49%).
- Unit tests: invariant-based placement tests (placed, in-bounds, no overlap,
  transverse centering), analysis tests (CG/GM, overweight rejection, top-heavy
  UNSTABLE detection), and parser edge cases (missing field, empty manifest,
  over-long line).
- Tooling: `make sanitize` (ASan+UBSan), `make analyze` (Clang static analyzer),
  `make debug`, `make format`, `WERROR=1`; `scripts/check.sh` one-command gate;
  `.clang-format`.
- CI: gcc + clang build matrix under `-Werror`, full test suite, Clang static
  analysis, and AddressSanitizer + UBSan over every example scenario.
- Documentation: README (with the naval-architecture GM math), CONTRIBUTING,
  `docs/BUG_JOURNAL.md`, and an MIT `LICENSE`.

### Changed
- Placement: unified `find_placement` / `commit_placement` into a single
  source-of-truth `Placement` decision so the two can no longer drift.
- Parser: validates that required ship-config fields are present and warns on
  unknown keys; over-long lines are drained and skipped instead of misparsed.

### Fixed
- **Uninitialized position fields** read as a "placed" sentinel — undefined
  behavior that let heap garbage influence the CG optimizer.
- **Out-of-bounds stow** — the new-shelf placement branch did not check the item
  fit the bin's length, so over-long cargo could be placed past the hull.
- **Divide-by-zero** in the draft / GM math when a required ship-config field
  (e.g. `width_m`) was missing.
- **Double-free** of the cargo array on a malformed-manifest parse error.

[Unreleased]: https://github.com/alikatgh/CargoForge-C/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/alikatgh/CargoForge-C/releases/tag/v1.0.0
