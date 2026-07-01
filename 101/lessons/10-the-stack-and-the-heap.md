# The stack and the heap

C gives you two fundamentally different places to store data: the **stack**, where memory is managed automatically, and the **heap**, where you manage it yourself. Understanding the difference is not optional — CargoForge-C uses heap allocation for the cargo array specifically because the number of cargo items is unknown until the manifest file has been read, and that is a problem the stack cannot solve.

## The mental model 🧠

The **stack** is a hotel notepad. Scribble a number on it, and the instant you leave the room — the moment a function returns — the maid tears off the page and throws it away. Automatic, effortless, gone. You never clean up after yourself, but you also can't keep anything past checkout.

The **heap** is a storage locker you rent. It stays yours, holding whatever you put inside, until *you* go back and unlock it to hand it back (`free`). CargoForge keeps the cargo array in a locker, not on the notepad, for one concrete reason: you don't know how many crates the manifest lists until you've read it, and you can't reserve a notepad page of unknown size — but you *can* rent a locker of exactly the right size once you know. The catch, which is Lesson 11's whole subject: forget to hand the locker back and you keep paying rent forever — a memory leak.

<svg viewBox="0 0 660 290" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:640px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Stack vs heap: the cargo pointer lives on the stack, the array it points to lives on the heap</title>
<desc>On the stack, main() holds a Ship struct whose cargo field is a pointer. That pointer points across to a block on the heap — the Cargo array allocated by malloc, whose size is known only after the manifest is read. The stack is freed automatically on return; the heap block you must free yourself.</desc>
<defs><marker id="sh-ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="#12A594"/></marker></defs>
<rect x="36" y="50" width="250" height="200" rx="8" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
<text x="40" y="40" fill="currentColor" font-size="12.5" font-weight="600">STACK · automatic</text>
<rect x="52" y="66" width="218" height="68" rx="5" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-width="1" opacity="0.7"/>
<text x="62" y="84" fill="currentColor" font-size="11.5" font-family="var(--md-code-font,monospace)">main()</text>
<text x="62" y="106" fill="currentColor" font-size="11.5">Ship ship;</text>
<rect x="178" y="94" width="84" height="26" rx="4" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.1"/>
<text x="220" y="111" fill="#12A594" font-size="10.5" text-anchor="middle" font-family="var(--md-code-font,monospace)">cargo •</text>
<rect x="52" y="146" width="218" height="40" rx="5" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-width="1" opacity="0.7"/>
<text x="62" y="170" fill="currentColor" font-size="11.5" font-family="var(--md-code-font,monospace)">parse_cargo_list()</text>
<text x="161" y="238" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.6">freed automatically on return</text>
<rect x="374" y="50" width="250" height="200" rx="8" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
<text x="378" y="40" fill="#12A594" font-size="12.5" font-weight="600">HEAP · you manage it</text>
<rect x="396" y="86" width="206" height="92" rx="5" fill="#12A594" fill-opacity="0.06" stroke="#12A594" stroke-width="1.2"/>
<text x="499" y="112" fill="currentColor" font-size="11.5" text-anchor="middle">Cargo[0]  Cargo[1]  …</text>
<text x="499" y="134" fill="currentColor" font-size="11.5" text-anchor="middle">Cargo[count-1]</text>
<text x="499" y="162" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.6" font-family="var(--md-code-font,monospace)">malloc(count·sizeof(Cargo))</text>
<text x="499" y="238" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.6">size known only after parsing — you free() it</text>
<path d="M262,107 C322,107 338,132 394,132" fill="none" stroke="#12A594" stroke-width="1.6" marker-end="url(#sh-ar)"/>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Stack** = "a scratchpad that the CPU cleans up for you when a function finishes" — every local variable in `safe_atof` (like `end` and `val`) lives here; they vanish automatically on return, so you never have to think about them.
- **Heap** = "a warehouse you rent yourself and must vacate yourself" — `parse_cargo_list` uses `malloc(count * sizeof(Cargo))` to rent exactly the right space for the cargo array after counting lines in the manifest; no one frees it for you.
- **Why the cargo array must be on the heap** = "the size isn't known until you read the file, so you can't declare it at compile time" — the stack requires the size to be a constant; because a manifest can have any number of cargo items, only a runtime `malloc` call can create the right-sized array.
- **`calloc` vs `malloc`** = "`calloc` hands you zeroed-out memory; `malloc` hands you whatever bytes were there before" — `parse_dg_field` uses `calloc` so that every field in the new `DGInfo` struct starts at zero, meaning absent manifest fields default to empty strings rather than garbage.
- **Dangling pointer / use-after-free** = "you freed the memory but forgot to stop pointing at it, so later code reads recycled bytes it doesn't own" — the bug in `parse_cargo_list` left `ship->cargo` pointing at freed memory after an error; the fix sets `ship->cargo = NULL` and `ship->cargo_count = 0` so `ship_cleanup`'s loop never touches it.
- **`realloc` for growing the stdin buffer** = "ask for a bigger room and move your stuff there if needed" — because stdin can't be rewound, `parse_cargo_list` starts with a small heap array and doubles it with `realloc` as lines arrive; the result is assigned to a temporary `new_lines` pointer first so the original isn't lost if the call fails.

**Why it matters:** getting the stack/heap boundary wrong in C is silent — the program compiles and sometimes even runs, but corrupts memory or crashes unpredictably later. Every rule in this lesson (check `malloc`'s return, `NULL` the pointer after `free`, zero the count) exists because the consequence of skipping it is a bug that only appears in production under the right input.

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

`parse_cargo_list` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) faces a concrete problem: a manifest file can have any number of cargo items, and that number is not known until the file has been read. A stack array is impossible here.

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

The fix is in both error paths in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c):

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

## Check yourself

??? question "Why can't the cargo array simply live on the stack?"
    Its size isn't known until the manifest file has actually been read line by line. The stack needs a size fixed at compile time (or at least at function entry); the heap lets you allocate exactly the right size once you've counted the lines.

??? question "Stack memory and heap memory differ mainly in who cleans them up — how?"
    Stack memory is freed automatically the instant the function that owns it returns. Heap memory stays allocated until the code explicitly calls `free()` on it — forget that call and it's a memory leak.

*Next: [malloc, free, and ownership](11-malloc-free-and-ownership.md).*
