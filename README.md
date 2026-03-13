# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/c-build.yml)](https://github.com/alikatgh/CargoForge-C/actions)

A pure C99 maritime cargo loading optimizer with IMO stability analysis. Zero external dependencies.

---

## Features

- **3D Guillotine Bin-Packing** — 6-orientation testing, best-fit heuristic, FFD sorting
- **Naval Architecture** — draft, KB, BM, KG, GM calculations with realistic hull coefficients
- **IMO Intact Stability** — GZ curve via wall-sided formula, area integration, MSC.267(85) compliance
- **Trim & Heel** — longitudinal trim from moment about midship, transverse heel from off-center CG
- **Cargo Constraints** — hazmat separation (IMDG), stacking weight limits, deck weight ratios, fragile protection
- **Multiple Output Formats** — human-readable, JSON, CSV, table, markdown
- **ASCII Visualization** — terminal-based cargo layout rendering

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
# Run optimization
./cargoforge optimize ship.cfg cargo.txt

# Validate inputs
./cargoforge validate ship.cfg cargo.txt

# Ship information
./cargoforge info ship.cfg cargo.txt

# JSON output
./cargoforge optimize ship.cfg cargo.txt --format=json

# CSV export
./cargoforge optimize ship.cfg cargo.txt --format=csv
```

Run `./cargoforge --help` for all options.

---

## Testing

```bash
# Unit tests (parser, analysis, constraints)
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
│   ├── main.c          # Entry point
│   ├── cli.c           # CLI parser & subcommands
│   ├── parser.c        # Ship config & cargo manifest parsing
│   ├── analysis.c      # Hydrostatics, trim/heel, IMO GZ curve
│   ├── placement_3d.c  # 3D guillotine bin-packing
│   ├── constraints.c   # Hazmat, stacking, deck weight constraints
│   ├── visualization.c # ASCII cargo layout
│   └── json_output.c   # JSON serialization
├── include/            # Header files
├── tests/              # Unit tests (21 tests across 3 suites)
├── examples/           # Sample ship configs and cargo manifests
├── Makefile
└── CMakeLists.txt
```

---

## Stability Analysis

CargoForge evaluates vessel stability per IMO MSC.267(85) Part A, Chapter 2.2:

| Criterion | Requirement |
|-----------|-------------|
| GM (initial) | >= 0.15 m |
| GZ at 30 deg | >= 0.20 m |
| Max GZ angle | >= 25 deg |
| Area 0-30 deg | >= 0.055 m-rad |
| Area 0-40 deg | >= 0.090 m-rad |
| Area 30-40 deg | >= 0.030 m-rad |

GZ curve computed using the wall-sided formula with trapezoidal integration.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. Pure C99, no external dependencies.

## License

MIT — see [LICENSE](LICENSE).
