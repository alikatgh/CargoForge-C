# Lab 11 - Link the library

In this lab you compile `libcargoforge` as a standalone C library and call the
engine from your own program — no CLI, no subprocess. After finishing, you will
know how to link against `libcargoforge.a`, navigate the public API in
[`include/libcargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/libcargoforge.h), and read the `CfResult` struct that every higher-level
tool (JSON output, the HTTP server, the WASM build) ultimately consumes.

---

## Prerequisites

- You completed Lab 4 (build system basics) and can run `make` from the repo root.
- `gcc` and `make` are installed (verify with `gcc --version`).
- You are in the repo root — the directory that contains the [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile) and `src/`.

---

## Step 1 — Build the library

The `lib` target compiles all engine sources (everything in `LIB_SRCS` inside the
[`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile)) into two artefacts: a static archive and a platform-specific shared
library.

```bash
make lib
```

**What to expect:**

```
mkdir -p build
gcc -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude -fPIC -c src/parser.c -o build/parser.o
gcc -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude -fPIC -c src/analysis.c -o build/analysis.o
... (several more .o lines) ...
ar rcs libcargoforge.a build/parser.o build/analysis.o ...
gcc -dynamiclib -install_name @rpath/libcargoforge.dylib -o libcargoforge.dylib ...
```

Confirm the artefacts exist:

```bash
ls -lh libcargoforge.a libcargoforge.dylib   # macOS
# or on Linux:
ls -lh libcargoforge.a libcargoforge.so
```

You should see both files, each several hundred kilobytes.

!!! note "What `-fPIC` means"
    Every object in `LIB_SRCS` is compiled with `-fPIC` (Position-Independent
    Code). This flag is required for the shared library but harmless for the
    static archive. The Makefile compiles everything with it so the same `.o`
    files serve both targets.

---

## Step 2 — Read the public header

Only one header file is needed by code that embeds the library:
[`include/libcargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/libcargoforge.h). Open it and skim the four sections:

- **Error codes** — `CF_OK` (0), negative values like `CF_ERR_PARSE` and
  `CF_ERR_OVERWEIGHT`.
- **`CargoForge *`** — an opaque handle. You never look inside it; you only
  pass it to API functions.
- **`CfResult`** — the plain-C struct that holds every output of an analysis:
  draft, KG, GM, trim, heel, all six IMO GZ criteria, and longitudinal-strength
  peaks.
- **`CfCargoInfo` / `CfIMDGViolation`** — read-only views into placed cargo and
  segregation violations.

The lifecycle every caller follows is always the same:

```
cargoforge_open → load ship → load cargo → optimize → read results → cargoforge_close
```

---

## Step 3 — Build the bundled example

The repo ships a ready-made example at [`examples/library_example.c`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/library_example.c). The
`example` Makefile target builds it and links it against `libcargoforge.a`:

```bash
make example
```

Internally that runs:

```bash
gcc -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
    -o examples/library_example \
    examples/library_example.c libcargoforge.a -lm
```

Key flags:
- `-Iinclude` — finds `libcargoforge.h`.
- `libcargoforge.a` — listed as a plain file (not `-l`), so `gcc` links the
  archive directly without needing it on `LD_LIBRARY_PATH`.
- `-lm` — the engine uses `<math.h>` internally (`sin`, `atan`, `sqrt`).

Run it:

```bash
./examples/library_example
```

**Expected output (values will vary slightly by platform):**

```
CargoForge Library Example (v3.0.0)

Placed: 5 / 5
Draft:  9.81 m
GM:     6.32 m (corrected: 6.32 m)
Trim:   0.001 m
Heel:   0.23 deg
IMO:    COMPLIANT

Cargo placements:
  BOX1         -> (0.0, 6.4, -8.0)
  BOX2         -> (54.0, 6.4, -8.0)
  BOX3         -> (12.0, 6.4, -8.0)
  FUEL         -> (0.0, 0.0, 0.0)
  FOOD         -> (126.0, 6.4, -8.0)

IMDG: 0 violation(s) — COMPLIANT

JSON output length: 1847 bytes
```

!!! note "Box-hull fallback"
    The example ship config string does not include a `hydrostatic_table` key,
    so the engine falls back to the box-hull model described in Lesson 19.
    Draft, KB, and BM are computed from first principles using `BLOCK_COEFF =
    0.75` and `WATERPLANE_COEFF = 0.85`. The `hydro_table_used` flag in
    `CfResult` will be `0`.

---

## Step 4 — Write your own caller

Create a new file `my_loader.c` in the repo root:

```c
/* my_loader.c — minimal libcargoforge caller */
#include "libcargoforge.h"
#include <stdio.h>

int main(void) {
    CargoForge *cf;
    if (cargoforge_open(&cf) != CF_OK) return 1;

    /* Ship: 120 m long, 22 m wide, 30 000 t capacity */
    if (cargoforge_load_ship_string(cf,
            "length_m=120\n"
            "width_m=22\n"
            "max_weight_tonnes=30000\n"
            "lightship_weight_tonnes=8000\n"
            "lightship_kg_m=6.0\n") != CF_OK) {
        fprintf(stderr, "%s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }

    /* Two cargo items */
    if (cargoforge_load_cargo_string(cf,
            "CRATE1 5000 10x2x2 standard\n"
            "CRATE2 3000  8x2x2 standard\n") != CF_OK) {
        fprintf(stderr, "%s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }

    if (cargoforge_optimize(cf) != CF_OK) {
        fprintf(stderr, "%s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }

    const CfResult *r = cargoforge_result(cf);
    printf("Draft %.2f m  GM %.2f m  IMO %s\n",
           r->draft,
           r->gm_corrected,
           r->imo_compliant ? "PASS" : "FAIL");

    cargoforge_close(cf);
    return 0;
}
```

Compile it:

```bash
gcc -O2 -std=c99 -Iinclude -o my_loader my_loader.c libcargoforge.a -lm
./my_loader
```

Expected output (box-hull, approximate):

```
Draft 4.87 m  GM 8.14 m  IMO PASS
```

!!! tip "Error propagation pattern"
    Every API function returns an integer: `CF_OK` (0) on success, a negative
    error code on failure. Always check the return value and call
    `cargoforge_errmsg(cf)` when it is not `CF_OK`. The error message is stored
    inside the handle — it is always safe to call even after a failure.

---

## Step 5 — Inspect the result struct

The `CfResult` pointer returned by `cargoforge_result` is valid until the next
`cargoforge_optimize`, `cargoforge_analyze`, `cargoforge_reset`, or
`cargoforge_close` call. It is not a copy — do not free it.

Add these lines before `cargoforge_close` in your caller to explore the stability
numbers:

```c
printf("KG=%.2f  KB=%.2f  BM=%.2f\n", r->kg, r->kb, r->bm);
printf("GZ@30=%.3f m  area_0_30=%.4f m·rad\n", r->gz_at_30, r->area_0_30);
printf("Trim=%.3f m  Heel=%.2f°\n", r->trim, r->heel);
```

Cross-check by hand: $GM = KB + BM - KG$. The printed `r->gm` should equal
`r->kb + r->bm - r->kg` to within floating-point rounding. Because no tanks are
defined, `r->free_surface_correction` is `0.0` and `r->gm_corrected == r->gm`.

---

## Step 6 — Run the full test suite against the library

The test binary `tests/test_library` exercises the public API end-to-end. Build
and run all 8 test binaries:

```bash
make test
```

Then rebuild with AddressSanitizer and UndefinedBehaviorSanitizer to catch memory
errors the same way the CI pipeline does:

```bash
make test-asan
```

**Expected final lines:**

```
--- Running All Tests ---
...
=== Memory safety tests (ASAN/UBSAN) passed ===
```

If any test fails under ASan, the sanitizer will print a stack trace identifying
the file and line number of the violation before aborting.

!!! warning "Do not skip test-asan"
    The `test-asan` run rebuilds everything from scratch with sanitizers enabled.
    It caught the real heap-use-after-free bug in `parse_cargo_list` (Lesson 13
    covers the full post-mortem). A clean `make test` is not sufficient on its
    own; always follow up with `make test-asan` on any new caller code.

---

## Step 7 — Get JSON output

`cargoforge_result_json` serializes the full `Ship` + `AnalysisResult` state to a
JSON string. The string is lazily allocated and cached inside the handle:

```c
const char *json = cargoforge_result_json(cf);
if (json) {
    /* Write to file, send over HTTP, parse with jq — anything */
    fputs(json, stdout);
}
```

The pointer is managed by the handle and must not be freed by the caller. It is
invalidated by the same calls that invalidate `CfResult *`.

To pretty-print it from the bundled example:

```bash
./examples/library_example | python3 -m json.tool 2>/dev/null | head -40
```

---

## Step 8 — Clean up

```bash
make clean
```

This removes `build/`, `libcargoforge.a`, `libcargoforge.dylib` (or `.so`),
`examples/library_example`, and all test binaries. Your hand-written `my_loader`
and `my_loader.c` live in the repo root and are not touched by `clean` — delete
them manually when done.

---

## Solution

The complete reference implementation for steps 4–5 combined:

```c
/* solution_lab11.c */
#include "libcargoforge.h"
#include <stdio.h>

int main(void) {
    CargoForge *cf;
    if (cargoforge_open(&cf) != CF_OK) return 1;

    if (cargoforge_load_ship_string(cf,
            "length_m=120\n"
            "width_m=22\n"
            "max_weight_tonnes=30000\n"
            "lightship_weight_tonnes=8000\n"
            "lightship_kg_m=6.0\n") != CF_OK) {
        fprintf(stderr, "ship: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf); return 1;
    }

    if (cargoforge_load_cargo_string(cf,
            "CRATE1 5000 10x2x2 standard\n"
            "CRATE2 3000  8x2x2 standard\n") != CF_OK) {
        fprintf(stderr, "cargo: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf); return 1;
    }

    if (cargoforge_optimize(cf) != CF_OK) {
        fprintf(stderr, "optimize: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf); return 1;
    }

    const CfResult *r = cargoforge_result(cf);
    printf("Draft  %.2f m\n",  r->draft);
    printf("KG     %.2f m\n",  r->kg);
    printf("KB     %.2f m\n",  r->kb);
    printf("BM     %.2f m\n",  r->bm);
    printf("GM     %.2f m  (check: KB+BM-KG = %.2f)\n",
           r->gm, r->kb + r->bm - r->kg);
    printf("GM_c   %.2f m  (FSC = %.3f m)\n",
           r->gm_corrected, r->free_surface_correction);
    printf("Trim   %.3f m  Heel %.2f deg\n", r->trim, r->heel);
    printf("GZ@30  %.3f m  area_0_30 %.4f m·rad\n",
           r->gz_at_30, r->area_0_30);
    printf("IMO    %s\n", r->imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");

    /* Per-item placements */
    for (int i = 0; i < cargoforge_cargo_count(cf); i++) {
        CfCargoInfo info;
        cargoforge_cargo_info(cf, i, &info);
        if (info.placed)
            printf("  %-8s placed at (%.1f, %.1f, %.1f)\n",
                   info.id, info.pos_x, info.pos_y, info.pos_z);
        else
            printf("  %-8s NOT PLACED\n", info.id);
    }

    cargoforge_close(cf);
    return 0;
}
```

Build and run:

```bash
gcc -O2 -std=c99 -Iinclude -o solution_lab11 solution_lab11.c libcargoforge.a -lm
./solution_lab11
```

---

## Recap

- `make lib` produces `libcargoforge.a` (static) and `libcargoforge.dylib/.so`
  (shared); link with the static archive using the pattern
  `gcc ... my_caller.c libcargoforge.a -lm`.
- [`include/libcargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/libcargoforge.h) is the only header an embedder needs; everything
  else is an implementation detail.
- The lifecycle is always: `cargoforge_open` → load ship → load cargo →
  `cargoforge_optimize` → read `CfResult *` → `cargoforge_close`.
- `CfResult` fields map directly to the physics: `r->kb + r->bm - r->kg ==
  r->gm`; `r->gm - r->free_surface_correction == r->gm_corrected`.
- Always validate callers with `make test-asan`; ASan caught the real
  heap-use-after-free in `parse_cargo_list` that would have silently corrupted
  production runs.
- `cargoforge_result_json` returns a lazily cached JSON string that is owned by
  the handle — do not free it.

*Next: [The whole pipeline, end to end](46-the-whole-pipeline.md).*
