# The library and public API

CargoForge-C is not only a command-line tool — it is also a reusable C library. `libcargoforge` packages the entire stability engine behind a clean, stable interface that any C (or C-compatible) program can call. This lesson explains how that library is built, what its API surface looks like, and how to drive it from your own code.

## The mental model 🧠

A library is the engine sold without the car around it. `make lib` packages the calculation core — `analysis.c`, `placement_3d.c`, `hydrostatics.c` — into `libcargoforge` and deliberately leaves `main.c` and `cli.c` behind, so any other program can call the physics directly instead of shelling out to a binary. The command-line tool becomes just *one* customer of the same engine.

A good public API is mostly about what it *hides*. Callers get an **opaque handle** — a `CargoForge *` they can hold but not open, because the struct is defined only inside `libcargoforge.c` — so the library can rearrange its internals without breaking anyone. They get **error codes** instead of exceptions (`CF_OK`, `CF_ERR_NO_SHIP`, `CF_ERR_STATE`) that even enforce call order, and a strict `open`/`close` lifecycle: `open` zeroes everything with `calloc`, `close` frees every sub-allocation. The contract is small, stable, and rigid on purpose — that is what makes it safe for strangers to build on.

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>libcargoforge: one engine behind a stable API, many consumers</title>
<desc>The calculation core is packaged as a library, leaving out main.c and cli.c. It exposes a stable public API — an opaque CargoForge handle and CF_ error codes — so the CLI, the server, the WebAssembly build, and any third-party program can all call the same engine.</desc>
<rect x="18" y="76" width="150" height="68" rx="6" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.45"/>
<text x="93" y="98" font-size="10" text-anchor="middle" fill="currentColor">engine core</text>
<text x="93" y="114" font-size="8" text-anchor="middle" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">analysis · placement</text>
<text x="93" y="126" font-size="8" text-anchor="middle" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">hydrostatics …</text>
<text x="93" y="139" font-size="7.5" text-anchor="middle" fill="#D05663" opacity="0.7">(no main.c / cli.c)</text>
<line x1="168" y1="110" x2="206" y2="110" stroke="currentColor" stroke-opacity="0.5"/><path d="M199,106 L206,110 L199,114" fill="none" stroke="currentColor" stroke-opacity="0.6"/>
<rect x="208" y="74" width="168" height="72" rx="6" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.2"/>
<text x="292" y="96" font-size="10.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">libcargoforge</text>
<text x="292" y="112" font-size="8.5" text-anchor="middle" fill="currentColor" opacity="0.65">.a / .so / .dylib</text>
<text x="292" y="128" font-size="8" text-anchor="middle" fill="#12A594" opacity="0.85">opaque CargoForge* · CF_ codes</text>
<g font-size="9.5" text-anchor="middle">
<rect x="430" y="22" width="150" height="32" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="505" y="42" fill="currentColor">CLI (cargoforge)</text>
<rect x="430" y="64" width="150" height="32" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="505" y="84" fill="currentColor">server (serve)</text>
<rect x="430" y="106" width="150" height="32" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="505" y="126" fill="currentColor">WebAssembly</text>
<rect x="430" y="148" width="150" height="32" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="505" y="168" fill="currentColor">third-party app</text>
</g>
<g stroke="currentColor" stroke-opacity="0.35">
<line x1="376" y1="104" x2="428" y2="38"/><line x1="376" y1="108" x2="428" y2="80"/><line x1="376" y1="114" x2="428" y2="122"/><line x1="376" y1="118" x2="428" y2="164"/>
</g>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Library vs. CLI** = "the same physics engine, packaged so other programs can call it directly" — `make lib` builds `libcargoforge.a` / `.so` / `.dylib` from the engine source files (`analysis.c`, `placement_3d.c`, `hydrostatics.c`, etc.) while deliberately leaving out `main.c` and `cli.c`, so the calculation core travels without its command-line wrapper.
- **Opaque handle (`CargoForge *`)** = "a box your code holds but cannot open" — the `CargoForge` struct is defined only inside `src/libcargoforge.c`; callers see only a pointer, so the library can rearrange its internals freely without breaking any code that links against it.
- **State machine / error codes** = "the API enforces the right order of operations" — `CF_ERR_NO_SHIP` fires if you call `cargoforge_load_cargo` before loading a ship, and `CF_ERR_STATE` fires if you call `cargoforge_check_imdg` before `cargoforge_optimize` or `cargoforge_analyze`; the integer codes (`CF_OK`, `CF_ERR_NOMEM`, …) are how a C function signals success or failure without exceptions.
- **`cargoforge_open` / `cargoforge_close`** = "allocate the session, then clean it up completely" — `open` uses `calloc` so every field starts at zero, and `close` calls `ship_cleanup` (freeing all cargo and hydrostatic sub-allocations) plus `free(cf->json_cache)` before freeing the handle itself; skipping `close` leaks everything the handle owns.
- **`fill_result`** = "a translation layer that keeps the public face stable" — after `perform_analysis` fills the internal `AnalysisResult` struct, `fill_result` copies selected fields into the public `CfResult` struct; this means internal reorganisations are absorbed here and callers never need to recompile.
- **`_string` loading variants** = "feed data from memory without touching the filesystem yourself" — `cargoforge_load_ship_string` and `cargoforge_load_cargo_string` use `mkstemp` to write a uniquely-named temp file, invoke the file-based loader, then `unlink` the file immediately, so web backends and test harnesses can pass in-memory text without managing temporary files.
- **JSON cache (`cf->json_cache`)** = "generate the JSON once, serve it for free after that" — `cargoforge_result_json` uses `open_memstream` to build the JSON into a heap buffer on the first call and stores the pointer in the handle; subsequent calls return the cached pointer until a new `optimize`, `reset`, or `close` invalidates it.

**Why it matters:** if you call operations out of order, skip `cargoforge_close`, or rely on internal headers instead of `libcargoforge.h`, you will get state errors, memory leaks, or ABI breakage the moment the engine internals change — the entire design of this library exists to make those failure modes impossible for a caller who follows the open → load → optimize → read → close pattern.

---

## Why a separate library?

The CLI (`cargoforge optimize`, `cargoforge serve`, …) is one consumer of the calculation engine. A ship-management system, a web backend, or a WebAssembly port might be another. Bundling the engine as a library separates the _what_ (the physics and placement algorithms) from the _how_ (command-line flags, JSON-RPC, WASM exports).

The Makefile captures this split cleanly:

```
make all    # builds the CLI binary only
make lib    # builds libcargoforge.a  (static)
            #        libcargoforge.dylib / libcargoforge.so  (shared)
```

The `lib` target compiles only the engine source — `parser.c`, `analysis.c`, `hydrostatics.c`, `tanks.c`, `placement_3d.c`, `constraints.c`, `imdg.c`, `longitudinal_strength.c`, `json_output.c`, and `libcargoforge.c` — and links them into both a static archive and a shared object. `cli.c`, `main.c`, and `server.c` are deliberately excluded; they belong to the CLI, not the library.

---

## The single public header

From [`include/libcargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/libcargoforge.h):

```c
#define CF_VERSION_MAJOR 3
#define CF_VERSION_MINOR 0
#define CF_VERSION_PATCH 0
#define CF_VERSION_STRING "3.0.0"
```

This is the only header an embedder needs to include. Everything the library exposes — types, error codes, function prototypes — lives here. Internal headers (`cargoforge.h`, `analysis.h`, `placement_3d.h`, etc.) are implementation details; they are not installed by `make install` and must not be relied on by library consumers.

The header is written in portable C99 and wraps itself in an `extern "C"` block so it compiles cleanly as C++ as well:

```c
#ifdef __cplusplus
extern "C" {
#endif
/* ... all declarations ... */
#ifdef __cplusplus
}
#endif
```

---

## The opaque handle

```c
typedef struct CargoForge CargoForge;
```

`CargoForge` is an **opaque type**: the struct definition is hidden inside [`src/libcargoforge.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/libcargoforge.c). Callers never see its fields; they only hold a pointer to it. This is the standard C pattern for encapsulation — the equivalent of a private class in C++.

Inside `libcargoforge.c`, the actual definition reveals what the handle carries:

```c
struct CargoForge {
    Ship            ship;
    int             ship_loaded;
    int             cargo_loaded;

    AnalysisResult  analysis;
    int             analyzed;

    IMDGCheckResult imdg;
    int             imdg_checked;

    CfResult        result;       /* public result cache */
    int             result_valid;

    char           *json_cache;   /* cached JSON output */
    char            errmsg[512];
};
```

All state lives _inside_ this struct. There are no global variables. Two handles in two threads are completely independent — provided each thread uses its own handle and does not share one.

---

## Error codes

```c
#define CF_OK               0
#define CF_ERROR           -1
#define CF_ERR_NOMEM       -2
#define CF_ERR_FILE        -3
#define CF_ERR_PARSE       -4
#define CF_ERR_NO_SHIP     -5
#define CF_ERR_NO_CARGO    -6
#define CF_ERR_OVERWEIGHT  -7
#define CF_ERR_STATE       -8
```

Every function that can fail returns one of these integers. `CF_OK` (0) is always success; negative values are failures. `cargoforge_errstr(code)` converts any code to a static human-readable string; `cargoforge_errmsg(cf)` returns the last error message stored in the handle (more specific, dynamically formatted).

!!! note
    The state machine enforced by the error codes matters. `CF_ERR_NO_SHIP` is returned by `cargoforge_load_cargo` if you try to load cargo before loading a ship. `CF_ERR_STATE` is returned by `cargoforge_check_imdg` if you call it before `cargoforge_optimize` or `cargoforge_analyze`. The API is deliberately strict: calling operations out of order is caught at runtime.

---

## Lifecycle: open, use, close

From [`src/libcargoforge.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/libcargoforge.c):

```c
int cargoforge_open(CargoForge **out) {
    if (!out) return CF_ERROR;

    CargoForge *cf = calloc(1, sizeof(CargoForge));
    if (!cf) return CF_ERR_NOMEM;

    cf->result.strength_compliant = -1;
    *out = cf;
    return CF_OK;
}

void cargoforge_close(CargoForge *cf) {
    if (!cf) return;
    if (cf->ship_loaded || cf->cargo_loaded)
        ship_cleanup(&cf->ship);
    if (cf->json_cache)
        free(cf->json_cache);
    free(cf);
}
```

`cargoforge_open` allocates the handle with `calloc` (zeroing all fields), sets `strength_compliant` to the sentinel value −1 (meaning "not checked"), and returns the pointer through an out-parameter. `cargoforge_close` calls `ship_cleanup` to free all cargo and hydrostatic sub-allocations, frees the cached JSON string, then frees the handle itself. Calling `cargoforge_close(NULL)` is a safe no-op.

`cargoforge_reset` clears the handle back to its initial state without freeing the handle memory — useful if you want to load a different ship in the same handle without allocating a new one.

---

## Loading data

The library offers two ways to feed data into a handle:

| Function | Input source |
|---|---|
| `cargoforge_load_ship(cf, path)` | File path |
| `cargoforge_load_ship_string(cf, text)` | In-memory string |
| `cargoforge_load_cargo(cf, path)` | File path |
| `cargoforge_load_cargo_string(cf, text)` | In-memory string |

The `_string` variants avoid the need to write a file to disk when data comes from a network request or is generated programmatically. Internally they use `mkstemp` to create a temporary file, write the string, call the file-based loader, then `unlink` the temp file:

```c
static int write_temp_file(const char *text, char *path, size_t pathsz) {
    snprintf(path, pathsz, "/tmp/cargoforge_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) return -1;

    size_t len = strlen(text);
    ssize_t written = write(fd, text, len);
    close(fd);

    if (written < 0 || (size_t)written != len) {
        unlink(path);
        return -1;
    }
    return 0;
}
```

`mkstemp` creates a file with a unique name derived from the template (`XXXXXX` is replaced with random characters) and returns an open file descriptor. The file is unlinked immediately after the loader returns, so it never accumulates on disk even if the process crashes after the `unlink` call.

---

## Running the engine

Three operations are available after loading:

```c
int cargoforge_optimize(CargoForge *cf);   // place cargo + analyse
int cargoforge_analyze(CargoForge *cf);    // analyse only (positions already set)
int cargoforge_check_imdg(CargoForge *cf); // IMDG segregation check
```

`cargoforge_optimize` is the primary call. It chains `place_cargo_3d` (the 3D bin-packing algorithm) with `perform_analysis` (the full stability calculation). Under the hood, from [`src/libcargoforge.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/libcargoforge.c):

```c
/* Run 3D bin-packing */
place_cargo_3d(&cf->ship);

/* Run stability analysis */
cf->analysis = perform_analysis(&cf->ship);
cf->analyzed = 1;

fill_result(cf);
```

`fill_result` copies fields from the internal `AnalysisResult` struct into the public `CfResult` struct. This translation step is what keeps the public ABI stable even if internal structs are reorganised.

`cargoforge_analyze` skips placement; it is intended for cases where the caller has already assigned positions to cargo items manually.

`cargoforge_check_imdg` runs the O(n²) all-pairs IMDG segregation check and requires `optimize` or `analyze` to have run first (enforced by the `CF_ERR_STATE` check).

---

## Reading results

### The `CfResult` struct

```c
const CfResult *cargoforge_result(const CargoForge *cf);
```

Returns a pointer to the cached result, or NULL if analysis has not run. The pointer is valid until the next `optimize`, `analyze`, `reset`, or `close` call. Key fields:

| Field | Meaning |
|---|---|
| `placed_count` / `total_count` | How many cargo items were placed vs. loaded |
| `draft` | Mean draft in metres |
| `gm` / `gm_corrected` | GM before / after free-surface correction |
| `trim` | Trim by stern (m); positive = stern deeper |
| `heel` | Heel angle in degrees |
| `imo_compliant` | 1 if all six IMO MSC.267(85) criteria pass |
| `max_shear_force` / `max_bending_moment` | Peak SWSF (t) and SWBM (t·m) |
| `strength_compliant` | 1 = pass, 0 = fail, −1 = not checked |

The relationship between the key hydrostatic quantities is:

$$GM_{corrected} = KB + BM - KG - FSC$$

where $FSC$ is the free-surface correction (`free_surface_correction` field). This is the value the IMO criteria are applied against.

### Per-item cargo info

```c
int cargoforge_cargo_count(const CargoForge *cf);
int cargoforge_cargo_info(const CargoForge *cf, int index, CfCargoInfo *info);
```

`CfCargoInfo` is a flat, read-only view of a single cargo item: id, weight, dimensions, type, whether it was placed (`placed = 1` when `pos_x >= 0`), its 3D position, and DG fields if present. The library fills this struct from the internal `Cargo` array — callers never touch `Cargo` directly.

### IMDG violations

```c
int cargoforge_imdg_violation_count(const CargoForge *cf);
int cargoforge_imdg_violation(const CargoForge *cf, int index, CfIMDGViolation *v);
int cargoforge_imdg_compliant(const CargoForge *cf);
```

Each `CfIMDGViolation` holds the indices of the two offending cargo items, the required segregation type, the actual distance between them, and a human-readable description. `cargoforge_imdg_compliant` returns −1 if the IMDG check has not been run.

### JSON output

```c
const char *cargoforge_result_json(CargoForge *cf);
```

Returns the full analysis as a JSON string. The first call generates the JSON into a heap buffer using POSIX `open_memstream` (which grows the buffer automatically as bytes are written) and caches the result in `cf->json_cache`. Subsequent calls return the cache immediately. The pointer is invalidated by `reset`, `close`, or any subsequent `optimize`/`analyze` call.

---

## A complete example

[`examples/library_example.c`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/library_example.c) demonstrates the full flow from open to close. The essential structure:

```c
#include "libcargoforge.h"
#include <stdio.h>

int main(void) {
    CargoForge *cf;
    if (cargoforge_open(&cf) != CF_OK) return 1;

    const char *ship =
        "length_m=180\n"
        "width_m=32\n"
        "max_weight_tonnes=50000\n"
        "lightship_weight_tonnes=12000\n"
        "lightship_kg_m=7.5\n";

    if (cargoforge_load_ship_string(cf, ship) != CF_OK) {
        fprintf(stderr, "Ship load error: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }
```

The error-handling idiom is consistent throughout: check the return code, call `cargoforge_errmsg` for the message, and call `cargoforge_close` before returning. Because `cargoforge_close(NULL)` is safe, you can simplify error paths with a `goto cleanup` pattern once you are comfortable with that idiom.

After loading cargo and calling `cargoforge_optimize`, the example reads the result struct directly:

```c
    const CfResult *r = cargoforge_result(cf);
    printf("Draft:  %.2f m\n", r->draft);
    printf("GM:     %.2f m (corrected: %.2f m)\n", r->gm, r->gm_corrected);
    printf("IMO:    %s\n", r->imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");
```

It then iterates over cargo positions using `cargoforge_cargo_info`, runs the IMDG check, and obtains the JSON string — all through the public API, with no access to internal headers.

To compile it:

```sh
make lib
gcc -Iinclude -o library_example examples/library_example.c -L. -lcargoforge -lm
./library_example
```

The `-Iinclude` flag puts [`include/libcargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/libcargoforge.h) on the include path. `-L.` tells the linker to look for `libcargoforge.a` (or `.so`/`.dylib`) in the current directory. `-lm` links the math library, which the stability calculations require for `sin`, `tan`, and `atan`.

!!! warning
    When using the shared library (`-lcargoforge` resolving to `.so` or `.dylib`), you must ensure the library is on the dynamic linker's search path at runtime (`LD_LIBRARY_PATH` on Linux, `DYLD_LIBRARY_PATH` on macOS, or installed to a standard prefix). The static library (`libcargoforge.a`) bundles everything into the binary and has no runtime dependency.

---

## How `fill_result` insulates the ABI

The internal `AnalysisResult` struct (in `cargoforge.h`) is free to change as the engine evolves. The public `CfResult` struct (in `libcargoforge.h`) is the stable interface. `fill_result` in [`src/libcargoforge.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/libcargoforge.c) is the explicit bridge:

```c
static void fill_result(CargoForge *cf) {
    const AnalysisResult *a = &cf->analysis;
    CfResult *r = &cf->result;

    r->placed_count   = a->placed_item_count;
    r->total_count    = cf->ship.cargo_count;
    r->gm             = a->gm;
    r->gm_corrected   = a->gm_corrected;
    r->imo_compliant  = a->imo_compliant;
    /* ... all other fields ... */
    cf->result_valid = 1;
}
```

Any internal reorganisation that does not change the public semantics can be absorbed here, without recompiling code that links against the library. This is the key discipline behind a stable ABI in C.

---

## Recap

- `make lib` builds `libcargoforge.a` and the shared object; [`include/libcargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/libcargoforge.h) is the only header callers need.
- `CargoForge *` is an opaque handle — all state is inside the struct, there are no globals, and two handles are fully independent.
- The API enforces a load → optimize → read state machine; out-of-order calls return `CF_ERR_STATE` or `CF_ERR_NO_SHIP`.
- `_string` loading variants write a temp file via `mkstemp` and `unlink` it after parsing, so callers can feed in-memory data without managing files.
- `fill_result` copies internal `AnalysisResult` fields into the public `CfResult`, insulating the library ABI from engine implementation changes.
- JSON output is lazily generated via `open_memstream` and cached inside the handle until it is invalidated by a new `optimize` or `reset` call.

*Next: [The JSON-RPC server](44-the-json-rpc-server.md).*
