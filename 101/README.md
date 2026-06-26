# CargoForge-C 101 — from zero to systems & naval-architecture engineer

A complete, self-paced course — **47 lessons across 12 modules, plus a hands-on lab at the
end of every module** — that takes someone with **no programming and no engineering
background** all the way to understanding a real maritime cargo-stowage and ship-stability
program. Every example is drawn from the **actual code in this repository**.

The labs aren't reading — you *write and run* code against the repo (build the tool, parse a
ship config, compute a center of gravity by hand and then in C, place cargo in 3D bins, break
the parser with a fuzzer and watch AddressSanitizer catch it, link the engine as a library).
Each has guided steps, expected output, and a full solution.

There are no toy examples. When we explain a metacentric height, we read the `perform_analysis`
function that the CLI actually runs. When we explain a use-after-free, it's the *real* one the
fuzzer found in `parse_cargo_list`. The engineering (memory, build systems, testing,
sanitizers) and the naval architecture (buoyancy, GM, free surface, IMO criteria) are taught
from scratch, so nothing is hand-waved.

## Two things at once

CargoForge-C sits at the intersection of two disciplines, and this course teaches both:

1. **Systems programming in C** — pointers, the stack and heap, manual memory management, the
   bug classes that follow (use-after-free, double-free, buffer overflow), build systems, unit
   testing, sanitizers, and fuzzing.
2. **Naval architecture** — why a ship floats (buoyancy, displacement, draft), why it stays
   upright (center of gravity, the metacenter, `GM = KB + BM − KG`), and how cargo placement,
   free-surface effects, and class-society limits decide whether a loading plan is safe.

## Run it

The lessons are plain Markdown served with **[MkDocs](https://www.mkdocs.org/) +
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)** — light/dark themes,
search, server-side syntax highlighting, and MathJax.

```bash
pip install -r 101/requirements.txt     # mkdocs-material
mkdocs serve -f 101/mkdocs.yml           # → http://localhost:8000
```

Use the sun/moon toggle in the header to switch **light/dark**. To build a static site for
hosting (e.g. GitHub Pages / Cloudflare Pages):

```bash
mkdocs build -f 101/mkdocs.yml           # → 101/site/
```

## The curriculum

| Module | Lessons | Focus |
|--------|---------|-------|
| — | Welcome | Orientation & the full map |
| **1 · Programming foundations (C)** | 01–04 | Why C, the toolchain, your first program, CLI/git/reproducibility |
| **2 · The C language in depth** | 05–08 | Structs/enums (the data model), the preprocessor & headers, strings, error handling |
| **3 · Memory & pointers** | 09–13 | Pointers, the stack & heap, `malloc`/`free` & ownership, arrays & bounds, the bug classes |
| **4 · Build, test, reproduce** | 14–16 | Make & CMake, unit testing in C, continuous integration |
| **5 · Naval architecture I — flotation** | 17–20 | Buoyancy & Archimedes, displacement/density/draft, the box-hull model, hydrostatic tables |
| **6 · Naval architecture II — stability** | 21–25 | Center of gravity, the metacenter, `GM = KB + BM − KG`, trim & heel, free surface & IMO criteria |
| **7 · Parsing & data** | 26–29 | Config & manifest formats, tokenizing in C, validation & robustness, dangerous-goods (IMDG) parsing |
| **8 · Algorithms — stowage & placement** | 30–33 | The stowage problem, First-Fit-Decreasing, 3D bins with weight limits, constraints |
| **9 · The stability engine in code** | 34–37 | The analysis module, hydrostatic interpolation, tanks & free surface, longitudinal strength |
| **10 · Quality engineering** | 38–41 | Sanitizers (ASan/UBSan), static analysis, fuzzing, coverage & benchmarks |
| **11 · Shipping it** | 42–45 | The CLI & output formats, the library & public API, the JSON-RPC server, WASM & on-device |
| **12 · Capstone & frontier** | 46–47 | The whole pipeline end to end, and where real classed stability software goes next |

Work them **in order** — each builds on the last. Module 3 (memory) and Module 6 (stability)
are the heart of the project; everything before sets them up, everything after builds on them.

## How it's structured

```
101/
├── mkdocs.yml              # site config: nav (12 modules), Material theme, math, code
├── requirements.txt        # mkdocs-material
└── lessons/                # docs_dir — the 47 lessons + 12 labs (plain Markdown)
    ├── index.md            # 00 · Welcome (the homepage)
    ├── 01-…  …  47-….md
    ├── lab-01-…  …  lab-12-….md
    ├── glossary.md
    └── assets/
        ├── extra.css       # restrained editorial overrides (hairlines, one accent, no shadows)
        ├── mathjax.js      # MathJax config for pymdownx.arithmatex
        └── sidebar-toggle.js
```
