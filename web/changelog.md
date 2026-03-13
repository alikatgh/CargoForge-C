# Changelog

All notable changes to CargoForge-C are documented here.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), using [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added — Phase 1: Maritime Calculation Engine

- **Hydrostatic Table Interpolation** — Load real stability booklet data from CSV
  - Linear interpolation for draft, KB, BM, KM, TPC, MTC, waterplane area, LCB
  - Reverse lookup: displacement to draft
  - Supports 7-column (minimal) and 9-column (full) CSV formats
  - Falls back to box-hull model when no table is configured
- **Free Surface Correction** — Tank configuration with FSM calculation
  - CSV-based tank definition (id, dimensions, position, fill fraction, density)
  - Rectangular tank FSM: rho * l * b^3 / 12
  - Virtual KG rise applied to GM for IMO criteria
  - Full and empty tanks produce zero free surface effect
- **Longitudinal Strength** — Still water shear force and bending moment
  - 21-station hull discretization with trapezoidal lightship distribution
  - Cargo weight distributed at actual placement positions
  - Tapered buoyancy distribution with bow/stern reduction
  - Permissible limits checking against class society values
- **IMDG Segregation Engine** — Full dangerous goods compliance
  - Complete 17x17 IMDG Code Table 7.2.4 segregation matrix (Amendment 41-22)
  - 9 classes with subdivisions (17 distinct entries)
  - 6 segregation levels: none, away from (3m), separated (6m), complete compartment (12m), longitudinal (24m), incompatible
  - DG info parsing from cargo manifest
  - Full pairwise compliance checking with violation reporting
- **4 new test suites**: hydrostatics, tanks, longitudinal strength, IMDG (215+ assertions)

### Changed

- Stability analysis conditionally uses hydrostatic table or box-hull fallback
- Tank weight and vertical moment included in displacement/KG calculations
- IMO criteria use free-surface-corrected GM
- JSON/markdown output includes corrected GM, free surface correction, longitudinal strength
- Proper memory management via `ship_cleanup()`

---

## [2.1.0] — 2025-11-17

### Added

- **Stdin Support** — Read from pipes using `-` as filename
- **Analyze Subcommand** — JSON results analyzer with recommendations
- **Configuration File** — Global (`~/.cargoforgerc`) and project (`.cargoforgerc`) defaults

---

## [2.0.0] — 2025-11-17

### Added

- Modern CLI with subcommand system (optimize, validate, info, interactive, version, help)
- Multiple output formats: JSON, CSV, Table, Markdown, Human-readable
- Interactive mode, validation, filtering, colored output
- Stdin/stdout piping for Unix integration

---

## [0.3.0-production]

### Added

- 3D bin-packing algorithm using guillotine heuristic
- Cargo constraints (hazmat separation, fragile, stacking)
- CMake build system, Doxygen config, memory safety testing

---

## [0.1.0-alpha]

### Added

- Initial 2D cargo placement using FFD algorithm
- Basic stability calculations
- Ship config and cargo manifest parsing
