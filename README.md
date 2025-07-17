# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/ci.yml)](https://github.com/alikatgh/CargoForge-C/actions)
[![Contributors](https://img.shields.io/github/contributors/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/issues)

A pure C simulator and optimizer for maritime cargo logistics, designed for maximum speed and hardware efficiency. Built to solve real-world challenges in shipping, such as optimal cargo placement for stability and route optimization on large vessels. Ideal for embedded systems or low-resource environments where performance is critical.

-----

## 🤔 Why CargoForge-C?

This project was inspired by a critical need in global shipping: **90% of world trade moves by sea**, and inefficient loading can cost millions in fuel and safety risks. This tool aims to simulate and optimize that process in pure C.

  - 🧠 **Learn and Contribute**: Perfect for C enthusiasts to practice low-level programming while tackling practical, high-impact logistics problems.
  - 🚢 **Maritime Focus**: Optimizes cargo loading on large vessels (e.g., container ships), considering weight distribution, stability, and efficiency to reduce fuel costs and safety hazards.
  - 💻 **Pure C for Performance**: No external libraries—everything is built from scratch for maximum portability and speed. It runs on any platform with a C compiler, from desktops to embedded boards like a Raspberry Pi (simulating shipboard computers).
  - 🌱 **Open for Growth**: Designed with a modular architecture to welcome contributors. Help add new features like hardware sensor integration or advanced optimization algorithms\!

## ✨ Features

  - ✅ **Cargo Loading Simulator**: Input ship specifications and a cargo manifest to receive an optimized cargo placement plan.
  - ✅ **Ship Stability Calculations**: Computes the vessel's center of gravity ($C\_g$) to ensure safe and stable loading.
  - ✅ **Route Optimization**: Includes a basic graph-based pathfinding algorithm for efficient voyages, with room for expansion.
  - ✅ **Performance-Oriented by Design**: Optimized for speed using inline functions, fixed-size arrays, and bitwise operations where appropriate.
  - 🔜 **Planned Extensions**: Future versions aim to include hardware interfaces (e.g., serial ports for sensors) and genetic algorithms for superior optimization.

## 🚀 Getting Started

### Prerequisites

  - A C compiler (e.g., GCC, Clang).
  - `make` (for easy building via the provided `Makefile`).

### Installation

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/alikatgh/CargoForge-C.git
    cd CargoForge-C
    ```

2.  **Build the project:**
    The `Makefile` compiles the main executable (`cargoforge`) with optimizations (`-O3` for speed).

    ```bash
    make
    ```

## ▶️ Usage

Run the simulator from the command line, passing configuration files for the ship and cargo.

```bash
./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
```

#### Input File Formats

  - **`sample_ship.cfg`**: Defines the vessel's dimensions and constraints.

    ```ini
    # Ship Specifications
    length_m=300
    width_m=48
    max_weight_tonnes=100000
    ```

  - **`sample_cargo.txt`**: A manifest listing all cargo items.

    ```
    # Cargo_ID Weight_Tonnes Dimensions(LxWxH)_meters Type
    ContainerA 20 6x2.5x2.5 standard
    ContainerB 25 6x2.5x2.5 hazardous
    ```

#### Example Output

The program will print the optimized load plan and performance metrics to the console.

```
Optimal Placement Plan:
- Bow:       ContainerA (CG Stability: 45%)
- Midship:   ContainerB
...
Total Execution Time: 0.12ms
```

For more examples, please see the `examples/` directory.

## 🤝 Contributing

Contributions are welcome and are the best way to make this project grow\! This project is designed to be a learning platform and a useful tool.

  - Check for issues labeled `good first issue` for beginner-friendly tasks.
  - Have an idea? Open an issue to discuss it.

#### How to Contribute

1.  Fork the repository.
2.  Create your feature branch (`git checkout -b feature/new-algorithm`).
3.  Commit your changes (`git commit -m "feat: Add Dijkstra route optimizer"`).
4.  Push to the branch (`git push origin feature/new-algorithm`).
5.  Open a Pull Request.

Please see `CONTRIBUTING.md` for detailed guidelines (coming soon).

## 🗺️ Roadmap

  - [ ] **v0.1**: Core simulator engine with file parsing and basic placement logic.
  - [ ] **v0.2**: Implement advanced optimization algorithms (e.g., genetic algorithms, simulated annealing).
  - [ ] **v0.3**: Add comprehensive stability and stress calculations.
  - [ ] **v1.0**: Full documentation, hardware integration extensions, and a stable API.

## ⚖️ License

This project is licensed under the MIT License - see the [LICENSE](https://www.google.com/search?q=LICENSE) file for details.

-----

Questions? Feel free to open an issue or reach out. Let's forge efficient logistics together\! ⚓