# Pointers and addresses

Every value in a C program lives at a specific location in memory — an **address**. A pointer is
a variable that stores one of those addresses. Once you understand that every variable has both
a value and a location, pointer syntax stops looking like line noise and starts making sense.
CargoForge-C passes ship data through pointers constantly; reading this module explains why,
and what `->` does when you see it in `analysis.c`.

---

## Every variable has an address

When the compiler allocates a variable, it picks a slot in memory and records where that slot
starts. The `&` operator returns that address:

```c
float draft = 7.5f;
float *p = &draft;   /* p holds the address of draft */
```

`float *` means "pointer to float" — a variable that holds the address of a `float`. The `*`
is part of the type, not a dereference here; it is only a dereference when it appears in an
*expression* (right-hand side, condition, function call argument).

To read or write through a pointer you **dereference** it with `*`:

```c
printf("%.2f\n", *p);   /* prints 7.5 */
*p = 8.0f;              /* changes draft to 8.0 */
```

`p` is the address. `*p` is the value at that address. They are different things.

---

## Why `perform_analysis` takes `const Ship *ship`

Look at the signature in `src/analysis.c`:

```c
AnalysisResult perform_analysis(const Ship *ship) {
```

`Ship` is a large struct. It holds a `cargo` array on the heap, a hydrostatic table, tank
data, and more. If the function accepted a `Ship` by value — `Ship ship` — C would copy every
byte of the struct onto the call stack each time the function ran. That is slow and wastes
memory. More importantly, a copied `ship->cargo` pointer would still point into the original
heap allocation; you would have two structs sharing a pointer to the same data, which is a
recipe for confusion.

Passing `const Ship *ship` instead means: "pass only the address of the Ship (8 bytes on a
64-bit machine), and promise not to modify it." The `const` qualifier is the promise — the
compiler will reject any attempt to write through `ship` inside the function.

The same pattern appears in `print_loading_plan`:

```c
void print_loading_plan(const Ship *ship) {
    AnalysisResult a = perform_analysis(ship);
```

One pointer threads through the whole analysis pipeline. The `Ship` data is never copied.

---

## `const` pointers — a read-only view

`const Ship *ship` means the *pointee* is read-only: you cannot write to the struct through
this pointer. The pointer itself can be changed (it could point at a different `Ship`). This
distinction matters in two positions:

| Declaration | What is read-only |
|---|---|
| `const Ship *ship` | the data the pointer points to |
| `Ship * const ship` | the pointer itself (cannot be reassigned) |
| `const Ship * const ship` | both |

CargoForge-C uses `const Ship *` (first form) almost everywhere. It is the correct choice for
any function that inspects but does not mutate the ship — it documents intent and lets the
compiler catch accidental writes.

!!! note
    When `const` is absent — `Ship *ship` — the function is signalling that it *may* modify
    the struct. `ship_cleanup` takes a non-const `Ship *` because it frees memory and sets
    fields to `NULL`. That mutation is exactly the point.

---

## Accessing struct members through a pointer: `->`

When you have a pointer to a struct, reaching a field uses `->` instead of `.`:

```c
Ship s;      /* a Ship by value  */
Ship *p = &s;

s.length     /* direct field access */
p->length    /* pointer field access — equivalent to (*p).length */
```

`p->length` is shorthand for `(*p).length`: first dereference the pointer to get the struct,
then access the field. The arrow operator bundles both steps.

You will see this constantly in `src/analysis.c`. The cargo loop is a clear example:

```c
for (int i = 0; i < ship->cargo_count; i++) {
    const Cargo *c = &ship->cargo[i];
    if (c->pos_x < 0) continue;

    r.total_cargo_weight_kg += c->weight;
    float cx = c->pos_x + c->dimensions[0] / 2.0f;
```

Line by line:

- `ship->cargo_count` — dereference `ship`, access `cargo_count`.
- `&ship->cargo[i]` — `ship->cargo` is a pointer to the first `Cargo`; indexing it with `[i]`
  gives the i-th `Cargo` by value; `&` then takes its address, giving `const Cargo *c`.
- `c->pos_x` — dereference `c` (a `Cargo *`) and access `pos_x`.

The `if (c->pos_x < 0) continue;` check is the sentinel test: unplaced cargo has
`pos_x = -1.0f`, so those items are skipped from the weight and moment accumulation.

---

## Pointers as output parameters

Sometimes a function needs to return more than one value. C cannot return multiple values
directly, but it can write to locations provided by the caller. `find_gz_max` uses this
pattern in `src/analysis.c`:

```c
static void find_gz_max(float gm, float bm, float *max_gz, float *max_angle) {
    *max_gz = 0.0f;
    *max_angle = 0.0f;
    for (float a = 1.0f; a <= 80.0f; a += 1.0f) {
        float gz = gz_at_angle(gm, bm, a);
        if (gz > *max_gz) {
            *max_gz = gz;
            *max_angle = a;
        }
    }
}
```

The caller passes the addresses of two floats it owns:

```c
find_gz_max(gm_effective, r.bm, &r.gz_max, &r.gz_max_angle);
```

Inside the function, `*max_gz = gz` writes through the pointer into `r.gz_max` back in
`perform_analysis`. When `find_gz_max` returns, `r.gz_max` and `r.gz_max_angle` hold the
results. This is the standard C idiom for multiple return values.

---

## Pointer-to-struct members that are themselves pointers

The `Ship` struct has several optional pointer fields — `hydro`, `tanks`, `strength_limits` —
that are set to `NULL` when not loaded. `perform_analysis` guards every use:

```c
if (ship->hydro && ship->hydro->loaded) {
    r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);
    HydroEntry he;
    hydro_interpolate(ship->hydro, r.draft, &he);
    r.kb = he.kb;
    r.bm = he.bm;
```

`ship->hydro` is itself a pointer (`struct HydroTable_ *`). The `&&` chains two checks: first
confirm the pointer is not NULL (i.e., a table was loaded), then access `ship->hydro->loaded`
to check the table parsed successfully. If either check fails, the code falls through to the
box-hull approximation.

Chaining `->` twice — `ship->hydro->loaded` — means: follow `ship` to reach the `Ship` struct,
read the `hydro` field (a pointer), follow that pointer to reach the `HydroTable` struct, then
read `loaded`.

!!! warning
    Dereferencing a NULL pointer is undefined behaviour and usually crashes the program. Always
    check `if (ptr)` or `if (ptr != NULL)` before using `->` on any pointer that might be NULL.
    The `ship->hydro &&` guard in `analysis.c` is the correct pattern.

---

## Cleanup: writing `NULL` after `free`

`ship_cleanup` in `src/analysis.c` shows the safe pattern for freeing heap memory through
pointer fields:

```c
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
```

After `free(ship->cargo)`, the memory is gone but the pointer field still contains the old
address. Any code that later reads `ship->cargo` without checking would access freed memory —
a **use-after-free** bug. Setting `ship->cargo = NULL` immediately prevents this: any
subsequent `if (ship->cargo)` guard will correctly see that no cargo is loaded.

This is not academic caution. The bug journal records that an earlier version of
`parse_cargo_list` freed `ship->cargo` in an error path without nulling the pointer, and
`ship_cleanup` would then iterate the freed array when it ran. AddressSanitizer caught it as
a `heap-use-after-free`.

---

## Summary of pointer syntax

| Syntax | Meaning |
|---|---|
| `T *p` | `p` is a pointer to type `T` |
| `&x` | address of variable `x` |
| `*p` | value at the address stored in `p` |
| `p->field` | shorthand for `(*p).field` |
| `const T *p` | pointer to read-only `T`; `p` itself may change |
| `T * const p` | read-only pointer; the data it points to may change |
| `if (p)` | true when `p` is not NULL |

---

## Recap

- Every variable occupies memory at an address; `&` retrieves that address, `*` follows it.
- Functions take `const Ship *ship` to avoid copying the struct and to signal read-only intent.
- `->` dereferences a pointer and accesses a field in a single step: `p->x` equals `(*p).x`.
- Output parameters (`float *max_gz`) let one function write multiple results back to the caller.
- Pointer fields like `ship->hydro` may be `NULL`; always guard with `if (ptr)` before using `->`.
- After `free(ptr)`, set `ptr = NULL` immediately; a dangling pointer is one of C's most common crashes.

*Next: [The stack and the heap](10-the-stack-and-the-heap.md).*
