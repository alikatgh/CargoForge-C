<div class="hero">
  <div class="hero-badge">Pure C99 &middot; Zero Dependencies</div>
  <h1>CargoForge-C</h1>
  <p class="hero-tagline">Maritime cargo loading optimizer with real hydrostatic analysis,<br>IMDG dangerous goods compliance, and IMO stability checking.</p>
  <div class="hero-actions">
    <a href="#/quickstart" class="btn-primary">Get Started</a>
    <a href="demo.html" class="btn-secondary" target="_blank">Interactive Demo</a>
  </div>
</div>

<div class="features-grid">
  <div class="feature-card">
    <div class="feature-icon">&#9875;</div>
    <h3>Hydrostatic Tables</h3>
    <p>Load real stability booklet CSV data. Linear interpolation for draft, KB, BM, KM, TPC, MTC, waterplane area, LCB.</p>
  </div>
  <div class="feature-card">
    <div class="feature-icon">&#9878;</div>
    <h3>IMDG Segregation</h3>
    <p>Full 17&times;17 dangerous goods matrix from IMDG Code Table 7.2.4. Six segregation levels with distance enforcement.</p>
  </div>
  <div class="feature-card">
    <div class="feature-icon">&#9881;</div>
    <h3>Longitudinal Strength</h3>
    <p>Still water shear force and bending moment across 21 hull stations. Permissible limits checking against class values.</p>
  </div>
  <div class="feature-card">
    <div class="feature-icon">&#9107;</div>
    <h3>Free Surface Correction</h3>
    <p>Tank FSM calculation with virtual KG rise. Rectangular tank model with density-aware free surface moments.</p>
  </div>
  <div class="feature-card">
    <div class="feature-icon">&#10003;</div>
    <h3>IMO Stability</h3>
    <p>GZ curve via wall-sided formula. Area under curve integration. Full MSC.267(85) Part A Ch.2.2 compliance.</p>
  </div>
  <div class="feature-card">
    <div class="feature-icon">&#9638;</div>
    <h3>3D Bin-Packing</h3>
    <p>Guillotine heuristic with 6-orientation testing. Best-fit placement with FFD sorting. Constraint-aware stacking.</p>
  </div>
</div>

---

## Quick Start

```bash
# Build
make

# Run
./cargoforge optimize ship.cfg cargo.txt

# Full analysis with hydrostatics + DG
./cargoforge optimize ship_full.cfg cargo_dg.txt --format=json
```

## Hydrostatic Modes

| Mode | When | Accuracy |
|------|------|----------|
| **Table interpolation** | `hydrostatic_table=` set in ship config | Matches stability booklet values |
| **Box-hull approximation** | No hydrostatic table loaded | Simplified estimates (Cb=0.75) |

## Project Structure

```
CargoForge-C/
├── src/
│   ├── main.c                    # Entry point
│   ├── cli.c                     # CLI parser & subcommands
│   ├── parser.c                  # Ship config, cargo, hydro/tank loading
│   ├── analysis.c                # Stability analysis, IMO criteria
│   ├── hydrostatics.c            # Hydrostatic table interpolation
│   ├── tanks.c                   # Tank parsing & free surface moments
│   ├── longitudinal_strength.c   # SWSF/SWBM across 21 hull stations
│   ├── imdg.c                    # IMDG Code segregation matrix
│   ├── placement_3d.c            # 3D guillotine bin-packing
│   ├── constraints.c             # Hazmat, stacking, deck weight
│   ├── visualization.c           # ASCII cargo layout
│   └── json_output.c             # JSON serialization
├── include/                      # Header files (one per module)
├── tests/                        # 7 test suites, 215+ assertions
├── examples/                     # Ship configs, cargo, hydro tables, tanks
├── Makefile
└── CMakeLists.txt
```
