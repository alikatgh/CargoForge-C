# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/ci.yml)](https://github.com/alikatgh/CargoForge-C/actions)
[![Contributors](https://img.shields.io/github/contributors/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/issues)

> **Project Status: Alpha**
> This project is in active development. Expect breaking changes until the v1.0 release.

A lightweight, dependency-free maritime cargo simulator written in pure C99.

-----

## Why CargoForge-C?

This project should and will address a critical need in global shipping: **90% of world trade moves by sea**, and inefficient loading costs millions in fuel and introduces safety risks. This tool aims to simulate and optimize that process.

  - **Learn and Contribute**: Perfect for C enthusiasts to practice low-level programming while tackling practical, high-impact logistics problems.
  - **Maritime Focus**: Optimizes cargo loading on large vessels, considering weight distribution and stability to reduce fuel costs and safety hazards.
  - **Performance & Portability**: No external libraries—everything is built from scratch. It runs on any platform with a C compiler, from desktops to embedded boards.
  - **Open for Growth**: Designed with a modular architecture to welcome contributors. Help add new features like hardware sensor integration or advanced optimization algorithms.

## Current Features

  - **Cargo Loading Simulator**: Input ship specifications and a cargo manifest to receive a 2D cargo placement plan using First-Fit Decreasing bin-packing algorithm.
  - **Ship Stability Calculations**: Computes the vessel's **center of gravity (CG)** and **metacentric height (GM)** for stability analysis.
  - **Robust Input Parsing**: Safe parsing with comprehensive error handling and validation for ship configs and cargo manifests.
  - **Modular Architecture**: Clean separation between parsing, placement optimization, and stability analysis modules.
  - **Zero Dependencies**: Built with only standard C99 library for maximum portability.

## Planned Features (Roadmap)

  - **3D Bin-Packing**: Full 3D cargo placement with height constraints
  - **Advanced Optimization**: Genetic algorithms and simulated annealing
  - **Cargo Constraints**: Handle cargo types (hazardous, refrigerated, fragile) with placement rules
  - **Route Optimization**: Graph-based pathfinding for voyage planning
  - **Hardware Integration**: Serial port interfaces for sensor data
  - **Real-time Visualization**: Interactive 3D cargo layout viewer

## Stack

  - **Language**: **C (C99)**
  - **Compiler**: Built and tested with **GCC** and **Clang**.
  - **Dependencies**: **None.** The project is intentionally self-contained for maximum portability.
  - **Build Systems**: Makefile and CMake supported
  - **Interfaces**: CLI + Interactive Web UI

## Getting Started

### Prerequisites

  - **C Compiler**: GCC or Clang (C99 standard)
  - **Build Tools**:
    - `make` (for Makefile builds)
    - `cmake` (optional, version 3.10+)
  - **For Web Interface**:
    - Python 3.8+ with pip
    - Flask and flask-cors (`pip install -r web/backend/requirements.txt`)
  - **Optional Tools**:
    - `doxygen` (for API documentation)
    - `valgrind` (for memory leak detection)
    - Docker (for containerized deployment)
  - **OS Compatibility**: Tested on Linux and macOS. Should compile on Windows via MinGW/MSYS2.

### Installation

#### Method 1: Makefile (Recommended)

```bash
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
make
```

#### Method 2: CMake

```bash
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

### Quick Start: Web Interface 🌐

The fastest way to try CargoForge-C is through the interactive web interface:

```bash
# Build the C binary
make

# Start the web server
cd web && ./START.sh

# Open browser to http://localhost:5000
```

**Web Features:**
- 🎮 Interactive 3D cargo visualization with Three.js
- 📊 Real-time stability analysis dashboard
- 🎯 Progressive training challenges (beginner → advanced)
- 🎨 Drag-and-drop cargo editor
- 📱 Mobile-friendly responsive design

See [web/README.md](web/README.md) for API documentation.

### Testing

```bash
# Run all unit tests
make test

# Run with memory sanitizers
make test-asan

# Run with Valgrind (requires valgrind installed)
make test-valgrind

# CMake testing
cd build && ctest --output-on-failure
```

## Usage

### Basic Usage

```bash
./cargoforge <ship_config.cfg> <cargo_list.txt> [options]
```

**Options:**
- `--no-viz`: Disable ASCII visualization output

**Examples:**

```bash
# Run with visualization
./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt

# Run without visualization
./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt --no-viz
```

### Output

The program generates:

1. **Placement Statistics** - 3D bin-packing results with weight distribution
2. **Stability Analysis** - Center of gravity, metacentric height, stability classification
3. **ASCII Visualization** - Top-down view of cargo layout
4. **Cargo Summary** - Detailed placement table with positions

## API Documentation

Generate API documentation with Doxygen:

```bash
doxygen Doxyfile
# Open docs/html/index.html in browser
```

## Contributing

Please see `CONTRIBUTING.md` for detailed guidelines. We welcome contributions in:
- Advanced optimization algorithms (genetic, simulated annealing)
- Real maritime dataset validation
- GUI development
- Performance improvements
- Bug fixes and testing

## Roadmap

  - [x] **v0.1-alpha**: Core 2D simulator engine, file parsing, and basic stability calculations
  - [x] **v0.2-beta**: 3D bin-packing, cargo constraints, comprehensive test suite, and API documentation (Current)
  - [ ] **v0.3**: Advanced optimization algorithms (genetic, simulated annealing), performance benchmarks
  - [ ] **v0.4**: Real maritime datasets, validation against IMO standards, SVG/web visualization
  - [ ] **v1.0**: Stable API, comprehensive documentation, hardware integration support

## Development

### Build Configurations

```bash
# Debug build with symbols
make clean && CFLAGS="-g -O0 -DDEBUG" make

# Release build (default)
make

# Memory safety testing
make test-asan           # AddressSanitizer + UBSan
make test-valgrind       # Valgrind leak checker
```

### Project Structure

```
CargoForge-C/
├── main.c                 # Entry point
├── parser.c/h            # Input file parsing
├── optimizer.c/h         # Optimization coordinator
├── placement_3d.c/h      # 3D bin-packing algorithm
├── placement_2d.c/h      # Legacy 2D placement
├── constraints.c/h       # Cargo constraints validation
├── analysis.c/h          # Stability calculations
├── visualization.c/h     # ASCII output
├── tests/                # Unit test suite
├── examples/             # Sample input files
├── Makefile              # Make build
├── CMakeLists.txt        # CMake build
└── Doxyfile              # API documentation config
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Naval architecture formulas based on IMO stability guidelines
- Bin-packing algorithms inspired by research in computational geometry
- Built as an educational tool for C programming and maritime logistics
