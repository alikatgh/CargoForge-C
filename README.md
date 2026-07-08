# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/c-build.yml)](https://github.com/alikatgh/CargoForge-C/actions)

**This is the project I built to learn C and naval architecture** — a pure C99 maritime
cargo-loading optimizer with real hydrostatic analysis, IMDG dangerous-goods compliance, and
IMO stability checking. Zero external dependencies. It sits at the intersection of two
disciplines I wanted to learn from scratch: **systems programming in C** (pointers, manual
memory, the bug classes that follow, build systems, sanitizers, fuzzing) and **naval
architecture** (why a ship floats, why it stays upright, and how cargo placement decides
whether a loading plan is safe).

Everything I learned building it is distilled into **[CargoForge-C 101](./101)** — a complete
course taught entirely from this repo's real code.

## The 101 course

**47 lessons across 12 modules, plus a hands-on lab at the end of every module.** It takes
someone with *no programming and no engineering background* all the way to understanding a
real ship-stability program — teaching both C and naval architecture from zero, so nothing is
hand-waved.

There are no toy examples. When the course explains metacentric height, you read the
`perform_analysis` function the CLI actually runs. When it explains a use-after-free, it's the
*real* one the fuzzer found in `parse_cargo_list`. The labs aren't reading — you write and run
code against the repo: build the tool, parse a ship config, compute a center of gravity by
hand and then in C, place cargo in 3D bins, break the parser with a fuzzer and watch
AddressSanitizer catch it.

| Module | Lessons | Focus |
|--------|---------|-------|
| **1 · Programming foundations (C)** | 01–04 | Why C, the toolchain, your first program, CLI/git/reproducibility |
| **2 · The C language in depth** | 05–08 | Structs/enums, the preprocessor & headers, strings, error handling |
| **3 · Memory & pointers** | 09–13 | Pointers, the stack & heap, `malloc`/`free` & ownership, arrays & bounds, the bug classes |
| **4 · Build, test, reproduce** | 14–16 | Make & CMake, unit testing in C, continuous integration |
| **5 · Naval architecture I — flotation** | 17–20 | Buoyancy & Archimedes, displacement/density/draft, the box-hull model, hydrostatic tables |
| **6 · Naval architecture II — stability** | 21–25 | Center of gravity, the metacenter, `GM = KB + BM − KG`, trim & heel, free surface & IMO criteria |
| **7 · Parsing & data** | 26–29 | Config & manifest formats, tokenizing in C, validation, dangerous-goods (IMDG) parsing |
| **8 · Algorithms — stowage & placement** | 30–33 | The stowage problem, First-Fit-Decreasing, 3D bins with weight limits, constraints |
| **9 · The stability engine in code** | 34–37 | The analysis module, hydrostatic interpolation, tanks & free surface, longitudinal strength |
| **10 · Quality engineering** | 38–41 | Sanitizers (ASan/UBSan), static analysis, fuzzing, coverage & benchmarks |
| **11 · Shipping it** | 42–45 | The CLI & output formats, the library & public API, the JSON-RPC server, WASM & on-device |
| **12 · Capstone & frontier** | 46–47 | The whole pipeline end to end, and where real classed stability software goes next |

### Run the course locally

The lessons are plain Markdown served with [MkDocs](https://www.mkdocs.org/) +
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/):

```bash
pip install -r 101/requirements.txt      # mkdocs-material
mkdocs serve -f 101/mkdocs.yml           # → http://localhost:8000
```

See [101/README.md](./101/README.md) for the full curriculum and structure.

## What CargoForge-C does

- **Hydrostatic table interpolation** — load real stability-booklet data from CSV; linear interpolation for draft, KB, BM, KM, TPC, MTC
- **Longitudinal strength** — still-water shear force and bending moment across 21 hull stations, checked against permissible limits
- **Free-surface correction** — tank configuration with FSM calculation and virtual KG rise for partially filled tanks
- **IMDG segregation engine** — full 17×17 dangerous-goods matrix (IMDG Code Table 7.2.4), 6 segregation types, stowage categories
- **IMO intact stability** — GZ curve via the wall-sided formula, area integration, MSC.267(85) compliance
- **3D guillotine bin-packing** — 6-orientation testing, best-fit heuristic, FFD sorting
- **Trim & heel** — longitudinal trim from moment about midship, transverse heel from off-center CG
- **Cargo constraints** — IMDG-aware hazmat separation, stacking weight limits, deck weight ratios, fragile protection
- **Multiple output formats** — human-readable, JSON, CSV, table, markdown
- **Box-hull fallback** — works without hydrostatic data using simplified coefficients; real data used when available

## Build

Requires GCC or Clang.

```bash
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
make                 # or: mkdir build && cd build && cmake .. && make
```

## Usage

```bash
# Basic optimization (box-hull model)
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt

# Full analysis with hydrostatic tables, tanks, and strength limits
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt

# With dangerous goods cargo
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt

# JSON output, or validate inputs
./cargoforge optimize ship.cfg cargo.txt --format=json
./cargoforge validate ship.cfg cargo.txt
```

Run `./cargoforge --help` for all options. See [USAGE.md](USAGE.md) for full documentation.

## Testing

```bash
make test            # 7 unit-test suites (parser, analysis, constraints, hydrostatics,
                     #   tanks, longitudinal strength, IMDG) — 215+ assertions
make test-asan       # AddressSanitizer + UBSan
make test-valgrind   # Valgrind leak checking
```

## Stability analysis

CargoForge supports two hydrostatic modes:

| Mode | When | Accuracy |
|------|------|----------|
| **Table interpolation** | `hydrostatic_table=` set in ship config | Matches stability-booklet values |
| **Box-hull approximation** | No hydrostatic table loaded | Simplified estimates (Cb=0.75) |

### IMO intact stability (MSC.267/85)

| Criterion | Requirement |
|-----------|-------------|
| GM (initial) | ≥ 0.15 m |
| GZ at 30° | ≥ 0.20 m |
| Max GZ angle | ≥ 25° |
| Area 0–30° | ≥ 0.055 m·rad |
| Area 0–40° | ≥ 0.090 m·rad |
| Area 30–40° | ≥ 0.030 m·rad |

GM is corrected for free-surface effects when tank data is loaded. The GZ curve is computed
with the wall-sided formula and trapezoidal integration. When permissible limits are
configured, still-water shear force and bending moment are checked across 21 hull stations.
DG cargo pairs are checked against the full IMDG Code Table 7.2.4 segregation matrix.

## Project structure

```
CargoForge-C/
├── src/
│   ├── main.c / cli.c            # Entry point & CLI subcommands
│   ├── parser.c                  # Ship config, cargo manifest, hydro/tank loading
│   ├── analysis.c                # Stability analysis, IMO criteria, ship_cleanup()
│   ├── hydrostatics.c            # Hydrostatic table CSV parsing & interpolation
│   ├── tanks.c                   # Tank parsing & free-surface moment calculation
│   ├── longitudinal_strength.c   # SWSF/SWBM across 21 hull stations
│   ├── imdg.c                    # IMDG Code segregation matrix & DG compliance
│   ├── placement_3d.c            # 3D guillotine bin-packing
│   ├── constraints.c             # IMDG-aware hazmat, stacking, deck weight
│   ├── visualization.c           # ASCII cargo layout
│   └── json_output.c             # JSON serialization
├── include/                      # Header files (one per module)
├── tests/                        # 7 test suites, 215+ assertions
├── examples/                     # Ship configs, cargo manifests, hydro tables, tank configs
├── 101/                          # The course (lessons + labs)
├── Makefile
└── CMakeLists.txt
```

## What building this taught me

The course is built around the scars — the parts of this repo that broke and taught something:

- **Manual memory management** — pointers, the stack vs. the heap, and `malloc`/`free`
  ownership, then the bug classes that follow when you get it wrong
  ([lesson 13](./101/lessons/13-memory-bugs.md)).
- **A real use-after-free** the fuzzer found in `parse_cargo_list`, and the `free(ptr)` /
  `ptr = NULL` discipline that fixes that whole class ([lesson 40](./101/lessons/40-fuzzing.md)).
- **Sanitizers as a daily tool** — ASan/UBSan catching memory errors the moment a test touches
  them ([lesson 38](./101/lessons/38-sanitizers.md)).
- **Why ships self-right** — deriving `GM = KB + BM − KG` from scratch and seeing it in the
  analysis code ([lesson 23](./101/lessons/23-gm-why-ships-self-right.md)).
- **Free surface and the IMO criteria** — why a half-full tank is dangerous, and how the code
  applies the correction ([lesson 25](./101/lessons/25-free-surface-and-imo-criteria.md)).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. Pure C99, no external dependencies. The
[capstone lesson](./101/lessons/lab-12-capstone.md) shows what a finished contribution looks like.

## License

MIT — see [LICENSE](LICENSE).
