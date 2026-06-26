# malloc, free, and ownership

C gives you direct control over memory — which means it also gives you the full
responsibility for it. This lesson explains how CargoForge-C allocates memory at
runtime with `malloc` and `calloc`, how it releases that memory with `free`, and
how `ship_cleanup` in `src/analysis.c` answers the central question every C
program must answer: *who frees this, and when?*

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
in `src/parser.c` handles this with a two-pass approach for regular files: count
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
Here is the hydrostatic table case from `src/parser.c`:

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
by `parse_dg_field` in `src/parser.c`:

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

This is a single, central cleanup function in `src/analysis.c`:

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

`perform_analysis` in `src/analysis.c` takes a `const Ship *` and returns an
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
