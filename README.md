# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/ci.yml)](https://github.com/alikatgh/CargoForge-C/actions)
[![Contributors](https://img.shields.io/github/contributors/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/issues)

> **Project Status: Alpha**
> This project is in active development. Expect breaking changes until the v1.0 release.

A tidy, beginner-friendly teaching prototype, but not yet a credible cargo-optimizer. The code is clean, fast, and dependency-free, yet it silently makes physics and safety assumptions that would fail in the real world. So let's fix it while learning C!

-----

## Why CargoForge-C?

This project should and will address a critical need in global shipping: **90% of world trade moves by sea**, and inefficient loading costs millions in fuel and introduces safety risks. This tool aims to simulate and optimize that process.

  - **Learn and Contribute**: Perfect for C enthusiasts to practice low-level programming while tackling practical, high-impact logistics problems.
  - **Maritime Focus**: Optimizes cargo loading on large vessels, considering weight distribution and stability to reduce fuel costs and safety hazards.
  - **Performance & Portability**: No external libraries—everything is built from scratch. It runs on any platform with a C compiler, from desktops to embedded boards.
  - **Open for Growth**: Designed with a modular architecture to welcome contributors. Help add new features like hardware sensor integration or advanced optimization algorithms.

## Initial features

  - **Cargo Loading Simulator**: Input ship specifications and a cargo manifest to receive an optimized cargo placement plan.
  - **Ship Stability Calculations**: Computes the vessel's **center of gravity ($C\_g$)** to ensure a safe and stable load.
  - **Route Optimization**: Includes a basic graph-based pathfinding algorithm for efficient voyages.
  - **Performance-Oriented**: Optimized for speed using inline functions, fixed-size arrays, and bitwise operations where appropriate.
  - **Planned Extensions**: Future versions will include hardware interfaces (e.g., serial ports) and genetic algorithms.

## Stack

  - **Language**: **C (C99)**
  - **Compiler**: Built and tested with **GCC** and **Clang**.
  - **Dependencies**: **None.** The project is intentionally self-contained for maximum portability.

## Getting Started

#### Prerequisites

  - A C compiler (GCC, Clang).
  - `make` (for easy building via the provided `Makefile`).
  - **OS Compatibility**: Tested on Linux and macOS. Should compile on Windows via MinGW/MSYS2.

#### Installation

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/alikatgh/CargoForge-C.git
    cd CargoForge-C
    ```

2.  **Build the project:**

    ```bash
    make
    ```

## Usage

Run the simulator from the command line with ship and cargo configuration files.

```bash
./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
```

#### Example(s)

```
Optimal Placement Plan:
- Bow:       ContainerA (CG Stability: 45%)
- Midship:   ContainerB
...
Total Execution Time: 0.12ms
```

> **Note**: If input files are missing or incorrectly formatted, the program will print a usage guide to the console.

## Contributing

Please see `CONTRIBUTING.md` for detailed guidelines (coming soon).

## Roadmap

  - [ ] **v0.1**: Core simulator engine and file parsing. (Target: Q3 2025)
  - [ ] **v0.2**: Advanced optimization algorithms (genetic, simulated annealing). (Target: Q4 2025)
  - [ ] **v0.3**: Comprehensive stability and hull stress calculations. (Target: Q1 2026)
  - [ ] **v1.0**: Full documentation, hardware integration extensions, and stable API. (Target: Q2 2026)

## License

MIT
