# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/c-build.yml)](https://github.com/alikatgh/CargoForge-C/actions)

A pure C99 maritime cargo loading optimizer with real hydrostatic analysis, IMDG dangerous goods compliance, and IMO stability checking. Zero external dependencies.

---

## Features

- **Hydrostatic Table Interpolation** — Load real stability booklet data from CSV; linear interpolation for accurate draft, KB, BM, KM, TPC, MTC
- **Longitudinal Strength** — Still water shear force and bending moment across 21 hull stations with permissible limits checking
- **Free Surface Correction** — Tank configuration with FSM calculation and virtual KG rise for partially filled tanks
- **IMDG Segregation Engine** — Full 17x17 dangerous goods matrix (IMDG Code Table 7.2.4), 6 segregation types, stowage categories
- **IMO Intact Stability** — GZ curve via wall-sided formula, area integration, MSC.267(85) compliance
- **3D Guillotine Bin-Packing** — 6-orientation testing, best-fit heuristic, FFD sorting
- **Trim & Heel** — longitudinal trim from moment about midship, transverse heel from off-center CG
- **Cargo Constraints** — IMDG-aware hazmat separation, stacking weight limits, deck weight ratios, fragile protection
- **Multiple Output Formats** — human-readable, JSON, CSV, table, markdown
- **Box-Hull Fallback** — Works without hydrostatic data using simplified coefficients; real data used when available

---

## Build

Requires GCC or Clang.

```bash
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
make
```

Or with CMake:

```bash
mkdir build && cd build
cmake ..
make
```

---

## Usage

```bash
# Basic optimization (box-hull model)
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt

# Full analysis with hydrostatic tables, tanks, and strength limits
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt

# With dangerous goods cargo
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt

# JSON output
./cargoforge optimize ship.cfg cargo.txt --format=json

# Validate inputs
./cargoforge validate ship.cfg cargo.txt
```

Run `./cargoforge --help` for all options. See [USAGE.md](USAGE.md) for full documentation.

---

## Testing

```bash
# All unit tests (7 suites: parser, analysis, constraints, hydrostatics, tanks, longitudinal strength, IMDG)
make test

# Memory safety (AddressSanitizer + UBSan)
make test-asan

# Valgrind leak checking
make test-valgrind
```

---

## Project Structure

```
CargoForge-C/
├── src/
│   ├── main.c                    # Entry point
│   ├── cli.c                     # CLI parser & subcommands
│   ├── parser.c                  # Ship config, cargo manifest, hydro/tank loading
│   ├── analysis.c                # Stability analysis, IMO criteria, ship_cleanup()
│   ├── hydrostatics.c            # Hydrostatic table CSV parsing & interpolation
│   ├── tanks.c                   # Tank parsing & free surface moment calculation
│   ├── longitudinal_strength.c   # SWSF/SWBM across 21 hull stations
│   ├── imdg.c                    # IMDG Code segregation matrix & DG compliance
│   ├── placement_3d.c            # 3D guillotine bin-packing
│   ├── constraints.c             # IMDG-aware hazmat, stacking, deck weight
│   ├── visualization.c           # ASCII cargo layout
│   └── json_output.c             # JSON serialization
├── include/                      # Header files (one per module)
├── tests/                        # 7 test suites, 215+ assertions
├── examples/                     # Ship configs, cargo manifests, hydro tables, tank configs
├── Makefile
└── CMakeLists.txt
```

---

## Stability Analysis

CargoForge supports two hydrostatic modes:

| Mode | When | Accuracy |
|------|------|----------|
| **Table interpolation** | `hydrostatic_table=` set in ship config | Matches stability booklet values |
| **Box-hull approximation** | No hydrostatic table loaded | Simplified estimates (Cb=0.75) |

### IMO Intact Stability (MSC.267/85)

| Criterion | Requirement |
|-----------|-------------|
| GM (initial) | >= 0.15 m |
| GZ at 30 deg | >= 0.20 m |
| Max GZ angle | >= 25 deg |
| Area 0-30 deg | >= 0.055 m-rad |
| Area 0-40 deg | >= 0.090 m-rad |
| Area 30-40 deg | >= 0.030 m-rad |

GM is corrected for free surface effects when tank data is loaded. GZ curve computed using the wall-sided formula with trapezoidal integration.

### Longitudinal Strength

When permissible limits are configured, CargoForge calculates still water shear force and bending moment across 21 hull stations and checks against class society limits.

### IMDG Dangerous Goods

Cargo items can carry DG class information. The engine checks all DG pairs against the full IMDG Code Table 7.2.4 segregation matrix (9 classes, 17 distinct rows, 6 segregation levels from "none" to "incompatible").

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. Pure C99, no external dependencies.

## License

MIT — see [LICENSE](LICENSE).
