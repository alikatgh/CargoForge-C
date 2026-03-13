# Quick Start

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

## Run

```bash
# Basic optimization (box-hull model)
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt

# Full analysis with hydrostatic tables, tanks, and strength limits
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt

# With dangerous goods cargo
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

You'll get a full stability analysis with cargo placement, trim/heel calculations, IMO compliance check, and (when configured) free surface correction and longitudinal strength verification.

## Test

```bash
# All unit tests (7 suites)
make test

# Memory safety (AddressSanitizer + UBSan)
make test-asan

# Valgrind leak checking
make test-valgrind
```

## Next Steps

- [Input Formats](input-formats.md) — How to write ship configs, cargo manifests, hydro tables, and tank configs
- [Stability Analysis](stability.md) — Understanding GM, trim, heel, and IMO criteria
- [Dangerous Goods](dangerous-goods.md) — IMDG segregation engine deep-dive
- [Interactive Demo](demo.md) — Try it in your browser
