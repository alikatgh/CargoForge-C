# CargoForge-C 101

> **From zero to systems & naval-architecture engineer — on a real codebase.**

This is a complete, self-paced course built around one real program: **CargoForge-C**, a
maritime cargo-stowage and ship-stability simulator written in C. You give it a ship and a
list of cargo; it decides where every item goes and reports whether the resulting load is
**safe enough to sail** — using the same physics naval architects use.

By the end you will be able to read every line of this repository and know *why* it's there:
the C, the memory management, the bin-packing algorithm, and the naval architecture.

## Who this is for

You need **no prior programming and no prior engineering knowledge**. If you can open a
terminal, you can start at Lesson 01. The course teaches the C language, the math, and the
naval architecture from the ground up — in the order you need them.

If you already know C, skim Modules 1–4 and start at Module 5 (the naval architecture). If
you already know ship stability, skim Modules 5–6 and focus on how it's *implemented*.

## What makes it different

- **No toy examples.** Every concept is anchored to code that actually runs. Attention to a
  `GM` value? We read [`perform_analysis`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c).
  A use-after-free? It's the *real* one a fuzzer found in `parse_cargo_list`.
- **Two disciplines, taught together.** Systems programming *and* naval architecture — because
  this project needs both, and the interesting bugs live where they meet.
- **Labs you run.** Each module ends with a hands-on lab: build the tool, compute a center of
  gravity by hand and then verify it in C, place cargo in 3D bins, fuzz the parser.

## The map

| Module | What you'll learn |
|--------|-------------------|
| **1 · Programming foundations (C)** | Why C exists, the compile→link→run model, and your first program. |
| **2 · The C language in depth** | Structs and enums (the `Ship`/`Cargo` data model), headers, strings, error handling. |
| **3 · Memory & pointers** | Pointers, stack vs heap, `malloc`/`free`, ownership — and the bugs that follow. |
| **4 · Build, test, reproduce** | Make & CMake, unit testing in C, CI. |
| **5 · Naval architecture I — flotation** | Buoyancy, displacement, draft, hydrostatic tables: *why ships float*. |
| **6 · Naval architecture II — stability** | Center of gravity, the metacenter, `GM = KB + BM − KG`: *why they stay upright*. |
| **7 · Parsing & data** | Turning text files into structs, robustly. |
| **8 · Algorithms — stowage & placement** | Bin packing, First-Fit-Decreasing, 3D bins, segregation rules. |
| **9 · The stability engine in code** | How the physics of Modules 5–6 becomes C. |
| **10 · Quality engineering** | Sanitizers, static analysis, fuzzing, coverage. |
| **11 · Shipping it** | The CLI, the library, the JSON-RPC server, WASM, on-device. |
| **12 · Capstone & frontier** | The whole pipeline end to end — and what real classed software adds. |

## How to use it

1. **Install once:** `pip install -r 101/requirements.txt`, then `mkdocs serve -f 101/mkdocs.yml`.
2. **Read in order.** Each lesson is short enough for one sitting and ends with a recap.
3. **Do the labs.** They're where the understanding sticks — open a terminal and run the code.
4. **Click any filename** in a lesson (e.g. a citation like `src/analysis.c`) to open the real source on GitHub at the cited line. Keep a clone open too if you want to build and run it.

Ready? Start with **[Lesson 01 · Why C, and the toolchain](01-why-c-and-the-toolchain.md)**.
