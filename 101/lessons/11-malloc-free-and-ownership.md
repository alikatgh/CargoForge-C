# malloc, free, and ownership

C gives you direct control over memory — which means it also gives you the full
responsibility for it. This lesson explains how CargoForge-C allocates memory at
runtime with `malloc` and `calloc`, how it releases that memory with `free`, and
how `ship_cleanup` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) answers the central question every C
program must answer: *who frees this, and when?*

## The mental model 🧠

Every `malloc` is a **loan**, and a loan has exactly one borrower who is on the hook to repay it.

The heap is a warehouse the operating system lends out by the byte. `malloc(n)` borrows `n` bytes and hands you a pointer — the *only* receipt that the loan exists. `free(p)` repays it. The borrowing is the easy part; the hard part is a whole program agreeing on **who repays each loan, and when**. That agreement is what "ownership" means, and C does not enforce it for you — you keep the discipline in your head.

In CargoForge-C that discipline is explicit. `parse_cargo_list` takes out the loan — `ship->cargo = malloc(...)` — and the rule is that the `Ship` now *owns* that block, so there is exactly one place it is ever repaid: `ship_cleanup`. Every memory bug in this codebase is a violation of that one rule: two owners both repay (double-free), nobody repays (leak), or someone repays and keeps spending the receipt anyway (the use-after-free from Lesson 13).

<svg viewBox="0 0 640 218" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:620px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Memory ownership: one malloc is balanced by exactly one free</title>
<desc>The happy path: parse_cargo_list calls malloc to borrow the cargo block, the Ship owns it, and ship_cleanup is the single place it is freed. Below, the two failure modes: a leak (no free) and a double-free (two frees of the same block).</desc>
<defs><marker id="own-t" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="#12A594"/></marker></defs>
<rect x="20" y="40" width="112" height="46" rx="6" fill="none" stroke="#12A594" stroke-width="1.4"/>
<text x="76" y="60" fill="#12A594" font-size="12" text-anchor="middle" font-family="var(--md-code-font,monospace)">malloc()</text>
<text x="76" y="76" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.6">borrow</text>
<rect x="178" y="40" width="150" height="46" rx="6" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.4"/>
<text x="253" y="60" fill="currentColor" font-size="11.5" text-anchor="middle" font-family="var(--md-code-font,monospace)">ship-&gt;cargo</text>
<text x="253" y="76" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">the Ship owns it</text>
<rect x="374" y="40" width="120" height="46" rx="6" fill="none" stroke="#12A594" stroke-width="1.4"/>
<text x="434" y="60" fill="#12A594" font-size="11.5" text-anchor="middle" font-family="var(--md-code-font,monospace)">ship_cleanup</text>
<text x="434" y="76" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.6">repay (free)</text>
<rect x="540" y="40" width="84" height="46" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.6"/>
<text x="582" y="67" fill="currentColor" font-size="11.5" text-anchor="middle" opacity="0.75">freed</text>
<line x1="132" y1="63" x2="176" y2="63" stroke="#12A594" stroke-width="1.4" marker-end="url(#own-t)"/>
<line x1="328" y1="63" x2="372" y2="63" stroke="#12A594" stroke-width="1.4" marker-end="url(#own-t)"/>
<line x1="494" y1="63" x2="538" y2="63" stroke="#12A594" stroke-width="1.4" marker-end="url(#own-t)"/>
<text x="320" y="110" fill="currentColor" font-size="11" text-anchor="middle" opacity="0.65">Exactly one malloc ↔ exactly one free. The Ship is the single owner.</text>
<line x1="20" y1="132" x2="624" y2="132" stroke="currentColor" stroke-width="1" opacity="0.15"/>
<text x="20" y="160" fill="#D05663" font-size="11.5" font-weight="600">When the rule breaks:</text>
<circle cx="30" cy="180" r="2.6" fill="#D05663"/><text x="44" y="184" fill="currentColor" font-size="11" opacity="0.8"><tspan fill="#D05663" font-weight="600">Leak</tspan> — the loan is never repaid (no free); the block is lost until the process exits.</text>
<circle cx="30" cy="202" r="2.6" fill="#D05663"/><text x="44" y="206" fill="currentColor" font-size="11" opacity="0.8"><tspan fill="#D05663" font-weight="600">Double-free</tspan> — two owners both repay the same block; the allocator's bookkeeping corrupts.</text>
</svg>

<div class="lesson-widget" data-widget="malloc-lifecycle-stepper"></div>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Heap allocation** = "ask the OS for a chunk of memory at runtime, sized however you need" — because `parse_cargo_list` cannot know at compile time how many `Cargo` items a manifest contains, it uses `malloc(count * sizeof(Cargo))` to request exactly the right amount after counting the lines.
- **`calloc` vs `malloc`** = "same allocation, but `calloc` zeroes every byte first" — CargoForge-C uses `calloc(1, sizeof(HydroTable))` for optional sub-structs so every field starts at 0 / NULL, making the "feature absent" state safe to read before anything is populated.
- **Ownership** = "the agreement about which part of the code is responsible for calling `free`" — in CargoForge-C the rule is simple: the `Ship` struct owns all heap memory it points to, and `ship_cleanup` is the single place that releases it all.
- **Dangling pointer** = "an address that still looks valid but points to memory already handed back to the heap" — this is why every `free` in `ship_cleanup` is immediately followed by setting the pointer to NULL; a NULL pointer crashes loudly, a dangling pointer silently corrupts data in unpredictable ways.
- **Double-free** = "calling `free` on the same pointer twice, which is undefined behavior" — nulling every pointer after freeing it means if `ship_cleanup` is ever called again, the `if (ship->cargo)` guard sees NULL and skips safely instead of freeing already-freed memory.
- **Error-path cleanup** = "undoing every allocation made so far before returning failure" — when `parse_cargo_list` hits an invalid weight midway through a manifest, it frees the DG pointers already parsed, frees the cargo array, and zeros both the pointer and the count so that `ship_cleanup` later sees a clean, NULL state rather than a dangling pointer.
- **Returning by value** = "copying all fields onto the caller's stack, leaving nothing on the heap" — `perform_analysis` returns an `AnalysisResult` struct containing only scalars, so there is nothing to free; this keeps the analysis layer stateless and ownership concerns entirely in the parser and `ship_cleanup`.

**Why it matters:** if ownership is ambiguous — either nothing frees a block (memory leak) or two things free it (double-free crash or silent corruption) — the program is broken even when it appears to work. Getting allocation and cleanup paired correctly is what separates a stable C program from one that fails unpredictably under load or on unusual manifests.

---

## Why static memory is not enough

A `Cargo` struct occupies a fixed amount of space. What you cannot know at
compile time is *how many* cargo items a particular manifest contains. The
number could be 3 or 3 000. You need a block of memory whose size is determined
at runtime — that is what heap allocation is for.

The heap is a region of memory managed by the C runtime. You request a chunk,
use it, and return it when you are done. The three essential functions live in
`<stdlib.h>`:

| Function | What it does |
|---|---|
| `malloc(n)` | Allocates `n` bytes. Contents are *undefined* (garbage). |
| `calloc(count, size)` | Allocates `count × size` bytes, zero-initialised. |
| `free(ptr)` | Returns the block to the heap. `ptr` must be the exact value returned by `malloc`/`calloc`. |

!!! note
    `calloc` is preferred when you need a struct to start zeroed. A freshly
    `calloc`-ed `HydroTable` has all fields at 0 / NULL, which is a safe
    initial state. `malloc` leaves those bytes uninitialized — reading them
    before writing is undefined behavior.

---

## Allocating the cargo array

The cargo manifest has an unknown length until the file is read. `parse_cargo_list`
in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) handles this with a two-pass approach for regular files: count
the non-comment lines on the first pass, then allocate exactly the right amount:

```c
/* from src/parser.c — file-based path */
int count = 0;
char line[MAX_LINE_LENGTH];
while (fgets(line, sizeof(line), file)) {
    if (line[0] != '#' && line[0] != '\n') count++;
}

ship->cargo = malloc(count * sizeof(Cargo));
if (!ship->cargo) {
    fprintf(stderr, "Error: Failed to allocate memory for cargo.\n");
    fclose(file);
    return -1;
}
ship->cargo_capacity = count;
ship->cargo_count = 0;
rewind(file);
```

`count * sizeof(Cargo)` asks for exactly as many bytes as `count` structs
require. The `sizeof` operator gives the size of one `Cargo` at compile time;
multiplying by the runtime count scales it correctly.

The NULL check immediately after `malloc` is not optional. On an out-of-memory
condition, `malloc` returns NULL. Writing through a NULL pointer is undefined
behavior — in practice, it crashes. Every heap allocation in this codebase is
checked.

---

## Allocating optional sub-structs

Not every ship has a hydrostatic table, a tank configuration, or structural
limits defined. These are represented as pointer fields in `Ship` that are NULL
by default and only allocated when the corresponding config key is present.
Here is the hydrostatic table case from [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c):

```c
/* from src/parser.c — loading the hydrostatic table */
if (hydro_path[0] != '\0') {
    ship->hydro = calloc(1, sizeof(HydroTable));
    if (ship->hydro) {
        if (parse_hydro_table(hydro_path, (HydroTable *)ship->hydro) != 0) {
            free(ship->hydro);
            ship->hydro = NULL;
            fprintf(stderr, "Warning: Failed to load hydrostatic table, "
                    "using box-hull fallback\n");
        }
    }
}
```

Three things to notice:

1. `calloc(1, sizeof(HydroTable))` — allocating a single struct and getting it
   zero-initialised in one call.
2. If the subsequent parse fails, `free` is called immediately and the pointer
   is set to NULL. The rest of the program sees NULL and falls back to the
   box-hull approximation. This is the pattern "allocate → try → rollback on
   failure."
3. The same pattern repeats for `ship->tanks` and `ship->strength_limits`. Each
   optional feature is either fully initialised or fully absent (NULL). There is
   no half-allocated state.

---

## The DG pointer: a heap value inside a struct

Each `Cargo` carries a field `dg` of type `DGInfo *`. For ordinary cargo it is
NULL. For dangerous goods it points to a heap-allocated `DGInfo` struct, created
by `parse_dg_field` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c):

```c
/* from src/parser.c — parse_dg_field */
static DGInfo *parse_dg_field(const char *field) {
    if (!field || strncmp(field, "DG:", 3) != 0)
        return NULL;

    DGInfo *dg = calloc(1, sizeof(DGInfo));
    if (!dg) return NULL;

    /* ... populate dg fields via strtok_r ... */

    /* Validate class range */
    if (dg->dg_class < 1 || dg->dg_class > 9) {
        free(dg);
        return NULL;
    }

    return dg;
}
```

This function *returns a heap pointer*. The caller (`parse_cargo_list`) stores it
in `c->dg`. The caller is now responsible for freeing it. This is the ownership
question made concrete: the allocation happened inside `parse_dg_field`, but the
lifetime is controlled by whoever holds the containing `Cargo`.

Note the early-exit rollback: if the IMDG class is out of range, `dg` is freed
before returning NULL. The invalid allocation never escapes the function.

---

## Ownership: who frees this?

Ownership is the agreement about which part of the code is responsible for
calling `free`. In CargoForge-C the rule is simple:

> **The `Ship` struct owns all heap memory it points to. `ship_cleanup` frees it.**

This is a single, central cleanup function in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c):

```c
/* from src/analysis.c — ship_cleanup */
void ship_cleanup(Ship *ship) {
    if (!ship) return;

    if (ship->cargo) {
        for (int i = 0; i < ship->cargo_count; i++) {
            if (ship->cargo[i].dg) {
                free(ship->cargo[i].dg);
                ship->cargo[i].dg = NULL;
            }
        }
        free(ship->cargo);
        ship->cargo = NULL;
    }
    if (ship->hydro) {
        free(ship->hydro);
        ship->hydro = NULL;
    }
    if (ship->tanks) {
        free(ship->tanks);
        ship->tanks = NULL;
    }
    if (ship->strength_limits) {
        free(ship->strength_limits);
        ship->strength_limits = NULL;
    }
}
```

The structure of this function reflects the ownership tree:

1. For each `Cargo` in the array, free the `dg` pointer (if non-NULL). This is
   the nested allocation that was created inside `parse_dg_field`.
2. Free the `cargo` array itself.
3. Free each optional top-level sub-struct.

Every `free` is followed by setting the pointer to NULL. This guards against a
second call to `ship_cleanup` accidentally freeing the same memory again
(a *double-free*, which is undefined behavior and a common security
vulnerability).

---

## Matching every malloc with one free: the error path problem

The hardest ownership question is not the happy path — it is what happens when
you have already allocated memory and then encounter an error. You must release
everything before returning failure, or that memory leaks.

`parse_cargo_list` allocates the cargo array before parsing individual lines.
If a weight field is invalid halfway through the manifest, it must free both
the DG pointers already parsed *and* the cargo array itself:

```c
/* from src/parser.c — error path for invalid weight */
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;   // avoid a dangling pointer -> use-after-free in ship_cleanup
    ship->cargo_count = 0;
    /* ... close file / free line buffer ... */
    return -1;
}
```

An identical block appears for invalid dimensions (parser.c lines 362–372).

The two lines after `free(ship->cargo)` are the critical ones:

```c
ship->cargo = NULL;
ship->cargo_count = 0;
```

Without them, `ship->cargo` would be a *dangling pointer* — an address pointing
to memory that has been returned to the heap. The `ship_cleanup` function would
later walk the loop `for (int i = 0; i < ship->cargo_count; i++)` and access
`ship->cargo[i].dg` through freed memory. This is a **heap-use-after-free**: the
use-after-free bug described in Lesson 12 was exactly this pattern. Setting the
pointer to NULL and the count to zero makes `ship_cleanup`'s guard (`if
(ship->cargo)`) a no-op — the cleanup becomes safe.

!!! warning
    A dangling pointer and a NULL pointer look the same to the author — neither
    is "good." The difference is that reading or writing through NULL crashes
    immediately and predictably. Reading or writing through a dangling pointer
    produces *silent corruption*: wrong results, intermittent failures, or a
    crash somewhere completely unrelated. Always NULL a pointer after freeing it.

---

## The analysis function owns nothing

`perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) takes a `const Ship *` and returns an
`AnalysisResult` by value:

```c
/* from src/analysis.c */
AnalysisResult perform_analysis(const Ship *ship) {
    AnalysisResult r;
    memset(&r, 0, sizeof(r));
    /* ... calculations ... */
    return r;
}
```

`AnalysisResult` is a plain struct containing only scalars (`float`, `int`). No
pointers, no heap allocations. Returning it by value copies all those fields onto
the caller's stack. There is nothing to free. This is not a coincidence: the
design keeps memory ownership entirely in the parser layer and cleanup in
`ship_cleanup`, so analysis can remain stateless and safe.

---

## Summary: the ownership map

```
Ship  (stack or outer allocation)
├── cargo[]       malloc'd array, freed by ship_cleanup
│   └── [i].dg   calloc'd DGInfo per DG item, freed by ship_cleanup before cargo
├── hydro         calloc'd HydroTable, freed by ship_cleanup
├── tanks         calloc'd TankConfig, freed by ship_cleanup
└── strength_limits  calloc'd StrengthLimits, freed by ship_cleanup
```

Each pointer in `Ship` is either NULL (feature absent) or points to exactly one
heap allocation. `ship_cleanup` visits each pointer, frees it, and NULLs it.
No allocation is freed twice; no allocation is forgotten.

---

## Recap

- `malloc(n)` allocates `n` uninitialized bytes; `calloc(count, size)` allocates
  and zeroes; both return NULL on failure — always check.
- Every allocation needs exactly one `free`. More than one is a double-free;
  zero is a memory leak.
- Ownership is the contract that says *who calls `free`*. In CargoForge-C, the
  `Ship` struct owns all its heap memory and `ship_cleanup` is the single
  release point.
- After calling `free`, set the pointer to NULL immediately. This converts a
  dangerous dangling pointer into a predictably NULL pointer and prevents
  use-after-free if the cleanup path runs again.
- Error paths must undo every allocation made so far — the parser's early-exit
  blocks free the DG pointers, free the cargo array, and zero both the pointer
  and the count before returning -1.

*Next: [Arrays, buffers, and bounds](12-arrays-buffers-and-bounds.md).*
