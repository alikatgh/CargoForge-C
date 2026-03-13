# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/c-build.yml)](https://github.com/alikatgh/CargoForge-C/actions)

A pure C99 maritime cargo loading simulator and optimizer. Zero external dependencies.

---

## Features

- **3D Guillotine Bin-Packing** — space-filling algorithm with 6-orientation testing
- **Naval Architecture Calculations** — block coefficient (Cb), waterplane coefficient (Cw), KB/BM/GM analysis
- **Cargo Constraints** — hazmat separation (3m minimum), point load limits, weight distribution rules
- **Stability Analysis** — IMO-compliant metacentric height calculations with status classification
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
./cargoforge info ship.cfg

# JSON output
./cargoforge optimize ship.cfg cargo.txt --format=json

# Interactive mode
./cargoforge interactive
```

Run `./cargoforge --help` for all options.

---

## Testing

```bash
# Unit tests
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
├── src/                # C source files
│   ├── main.c          # Entry point
│   ├── cli.c           # CLI parser & subcommands
│   ├── parser.c        # Input parsing
│   ├── analysis.c      # Stability calculations
│   ├── placement_3d.c  # 3D bin-packing (guillotine)
│   ├── placement_2d.c  # 2D placement
│   ├── constraints.c   # Cargo constraint validation
│   ├── visualization.c # ASCII output
│   └── json_output.c   # JSON serialization
├── include/            # Header files
├── tests/              # Unit tests
├── examples/           # Sample input files
├── Makefile            # Make build
└── CMakeLists.txt      # CMake build
```

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. Pure C99, no external dependencies.

## License

MIT — see [LICENSE](LICENSE).
