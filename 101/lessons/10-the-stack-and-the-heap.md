# The stack and the heap

C gives you two fundamentally different places to store data: the **stack**, where memory is managed automatically, and the **heap**, where you manage it yourself. Understanding the difference is not optional — CargoForge-C uses heap allocation for the cargo array specifically because the number of cargo items is unknown until the manifest file has been read, and that is a problem the stack cannot solve.

---

## Two memory regions, two lifetimes

### The stack: automatic, scoped, fast

Every time a function is called, the runtime reserves a block of memory on the stack for that function's local variables. When the function returns, that block is released — automatically, with no code required from you.

```c
// from src/parser.c
static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    float val = strtof(s, &end);
    // ...
    return val;
}
```

`end` and `val` live on the stack. When `safe_atof` returns, they are gone. This is not a problem: the caller only needs the return value, not the intermediate variables. Stack allocation is essentially free — the CPU just moves a pointer.

The constraint is that the **size must be fixed at compile time**. You can declare `char line[MAX_LINE_LENGTH]` on the stack because `MAX_LINE_LENGTH` is a compile-time constant. You cannot declare `Cargo cargo[n]` where `n` is a value read at runtime.

### The heap: manual, unbounded, your responsibility

The heap is a large region of memory managed by the C runtime. You request a block with `malloc` or `calloc`; you release it with `free`. The block persists until you call `free` or the program exits — regardless of which function requested it.

```c
// from src/parser.c (parse_ship_config)
ship->hydro = calloc(1, sizeof(HydroTable));
if (ship->hydro) {
    if (parse_hydro_table(hydro_path, (HydroTable *)ship->hydro) != 0) {
        free(ship->hydro);
        ship->hydro = NULL;
    }
}
```

`calloc(1, sizeof(HydroTable))` allocates enough bytes for one `HydroTable` struct on the heap and zeroes every byte. If the subsequent parse fails, `free` releases it immediately and the pointer is set to `NULL`. If it succeeds, the allocation outlives `parse_ship_config` — `ship->hydro` is still valid when `perform_analysis` reads it later.

Two things to notice:

1. The heap allocation is tied to `ship`, which itself was passed in by the caller and lives longer than the current function.
2. Setting the pointer to `NULL` after `free` is not just style — it prevents the freed address from being accidentally dereferenced later.

---

## Why the cargo array must be on the heap

`parse_cargo_list` in `src/parser.c` faces a concrete problem: a manifest file can have any number of cargo items, and that number is not known until the file has been read. A stack array is impossible here.

The function solves this with a **two-pass strategy for files** and a **dynamic buffer for stdin**:

### File path: count then allocate

```c
// from src/parser.c (parse_cargo_list)
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

Pass one counts non-comment, non-blank lines. After the loop, `count` is known, so `malloc(count * sizeof(Cargo))` allocates exactly the right number of `Cargo` structs. Pass two fills them in. The result is stored in `ship->cargo`, which outlives the function.

Notice the failure path: if `malloc` returns `NULL` (out of memory), the function closes the file and returns `-1`. It does not attempt to continue. This is the correct pattern — never use the return value of `malloc` without checking it.

### Stdin path: grow on demand

Stdin cannot be rewound, so the two-pass approach is impossible. Instead, `parse_cargo_list` reads all lines into a heap-allocated array of strings, doubling capacity when needed:

```c
// from src/parser.c (parse_cargo_list, stdin branch)
lines = malloc(line_capacity * sizeof(char*));
// ...
if (line_count >= line_capacity) {
    line_capacity *= 2;
    char **new_lines = realloc(lines, line_capacity * sizeof(char*));
    if (!new_lines) {
        for (int i = 0; i < line_count; i++) free(lines[i]);
        free(lines);
        return -1;
    }
    lines = new_lines;
}
lines[line_count] = strdup(line);
```

`realloc` either extends the existing allocation in-place or moves it to a new location, returning a fresh pointer. The old pointer must be discarded — using it after `realloc` succeeds is undefined behaviour. The new pointer is stored in `new_lines` first; if `realloc` fails it returns `NULL` without touching `lines`, so the pattern is:

```c
char **new_lines = realloc(lines, new_size);
if (!new_lines) { /* handle failure, lines still valid */ }
lines = new_lines;
```

Assigning directly into `lines = realloc(lines, ...)` would lose the original pointer on failure.

---

## Per-item heap allocation: DG info

Not every cargo item carries dangerous-goods data. The `Cargo` struct stores a pointer:

```c
struct DGInfo_ *dg;   // NULL for non-DG cargo
```

When `parse_cargo_list` encounters a DG field, `parse_dg_field` allocates a `DGInfo` struct and returns it:

```c
// from src/parser.c (parse_dg_field)
DGInfo *dg = calloc(1, sizeof(DGInfo));
if (!dg) return NULL;
// ... fill fields ...
if (dg->dg_class < 1 || dg->dg_class > 9) {
    free(dg);
    return NULL;
}
return dg;
```

If the IMDG class is out of range (not 1–9), the struct is freed before returning `NULL`. If valid, the allocation is returned to the caller and stored in `c->dg`. Every non-NULL `dg` pointer in the cargo array represents a separate heap allocation. All of them must be freed before `ship->cargo` itself is freed.

---

## The dangling-pointer bug this pattern almost caused

When `parse_cargo_list` encounters an invalid weight or bad dimensions part-way through a manifest, it must clean up and return an error. The original code freed `ship->cargo` but left `ship->cargo_count` at its current (non-zero) value. Later, `ship_cleanup` would loop:

```c
for (int i = 0; i < ship->cargo_count; i++) free(ship->cargo[i].dg);
```

`ship->cargo` pointed to freed memory. Reading through it is a **heap-use-after-free** — the memory might have been reused by another allocation, producing silent corruption or, with AddressSanitizer enabled, a crash.

The fix is in both error paths in `src/parser.c`:

```c
// from src/parser.c (parse_cargo_list, error path)
for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
free(ship->cargo);
ship->cargo = NULL;    // avoid a dangling pointer -> use-after-free/double-free in ship_cleanup
ship->cargo_count = 0;
```

Setting the pointer to `NULL` and the count to zero before returning ensures that `ship_cleanup`'s loop body is never entered on already-freed memory. This is a general rule: **free the contents, then the container, then NULL the pointer**.

!!! warning "The invariant"
    After any error path that calls `free(ship->cargo)`, two things must be true before the function returns: `ship->cargo == NULL` and `ship->cargo_count == 0`. Violating either breaks the cleanup contract.

---

## `calloc` vs `malloc`: zeroing matters

`parse_ship_config` uses `calloc` for its allocations (`ship->hydro`, `ship->tanks`, `ship->strength_limits`). `parse_dg_field` also uses `calloc`. Both functions then selectively fill in fields.

The difference:

| Function | Zeroes memory? | Common use |
|---|---|---|
| `malloc(n)` | No | When you will write every byte before reading |
| `calloc(count, size)` | Yes | Structs where unset fields should be 0/NULL |

For structs like `DGInfo` that contain pointer fields (`ems`, `un_number` as arrays), `calloc` ensures every field starts at zero before `strtok_r` writes into the ones that are present. If the EmS field is absent from the manifest line, `dg->ems` remains an empty string rather than random bytes.

---

## The stack as a temporary workspace

The stack is not just for local scalars. `parse_cargo_list` declares:

```c
char line[MAX_LINE_LENGTH];
```

inside the function. This is a fixed-size buffer on the stack. It is reused on every iteration of the read loop — overwritten by `fgets` each time. Putting it on the stack avoids a `malloc`/`free` per line and keeps the code simple.

Similarly, `parse_dg_field` works on a 64-byte stack copy of the DG field:

```c
char buf[64];
strncpy(buf, field + 3, sizeof(buf) - 1);
```

`strtok_r` modifies the string in-place by inserting `'\0'` bytes. Working on a stack copy instead of the original manifest line preserves the caller's data.

!!! note "When to use the stack vs the heap"
    - Size known at compile time and small enough (< a few KB): stack.
    - Size known only at runtime, or data must outlive the current function: heap.
    - Data needs to be shared across multiple modules via a pointer in a long-lived struct: heap.

---

## Recap

- Stack memory is allocated and freed automatically at function entry/exit; size must be fixed at compile time.
- Heap memory is allocated with `malloc`/`calloc` and freed with `free`; it persists until explicitly released.
- CargoForge-C allocates `ship->cargo` on the heap because the item count is unknown until the manifest is parsed.
- Per-item `DGInfo` structs are individually heap-allocated and must be freed before the containing array.
- After every `free`, set the pointer to `NULL` and any associated count to `0` — this is what prevents the heap-use-after-free that the fuzzer caught in `parse_cargo_list`.
- Use `calloc` when unset struct fields must default to zero or NULL; use `malloc` when you will write every byte before reading.

*Next: [malloc, free, and ownership](11-malloc-free-and-ownership.md).*
