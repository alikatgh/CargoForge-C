# Arrays, buffers, and bounds

C gives you raw control over memory — no safety net, no automatic resizing, no
index checking. Understanding how arrays work at the machine level is what
separates code that runs reliably from code that corrupts memory silently.
This lesson traces that understanding through [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c), where CargoForge-C
reads an entire cargo manifest into a heap-allocated array before a single item
is analysed.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Array** = "a row of same-size boxes sitting back-to-back in memory, numbered from zero" — C finds box `i` by jumping exactly `i` box-widths from the start; there is no guard at the end, so writing past the last box silently damages whatever comes next.
- **Fixed-size buffer (`char id[32]`)** = "a reserved slot of exactly 32 bytes baked into every `Cargo` struct" — `parse_cargo_list` copies user-supplied strings into it with `strncpy(..., sizeof(c->id) - 1)` plus an explicit `'\0'` write so a 31-character input never leaves the buffer unterminated.
- **Count-then-allocate (two-pass)** = "count the lines first, then ask for exactly that much memory" — `parse_cargo_list` rewinds the file after pass 1 so `malloc` gets a precise size instead of a wasteful upper-bound guess; `cargo_capacity` records how many slots exist while `cargo_count` tracks how many are actually filled.
- **Off-by-one** = "stopping one index too late (or too early) so you touch memory you don't own" — the dimension loop uses `d < MAX_DIMENSION`, not `d <= MAX_DIMENSION`, to keep `d` inside the three valid indices of `dimensions[3]`.
- **`realloc` double-assignment** = "save the new pointer before overwriting the old one, so a NULL return doesn't lose the original allocation" — the stdin path assigns into `new_lines` first, checks for NULL, and only then moves it into `lines`; skipping this step leaks every line already read.
- **Use-after-free / NULL-out discipline** = "after `free(ship->cargo)`, set the pointer to NULL and zero the count so nothing can accidentally use the freed memory" — every error path in `parse_cargo_list` does both assignments; missing either one is a bug that AddressSanitizer (`make test-asan`) will catch.

**Why it matters:** an out-of-bounds write or a dangling pointer doesn't crash at the bad line — it silently corrupts unrelated memory and surfaces as a mysterious crash or wrong result somewhere else entirely, making it one of the hardest bug classes to diagnose without a sanitizer.

---

## The mental model 🧠

You'll forget the rules — hold THIS picture instead:

> Imagine a row of numbered post-office boxes bolted to a wall. Each box is the same size. The clerk finds box 7 by counting exactly 7 boxes from the left — no list, no check, no guard. Box 32 on a 32-box wall? The clerk walks straight past the end and opens whatever's mounted there. It might be the manager's private cabinet. It might be thin air. The wall doesn't stop him.

That's `ship->cargo`: a row of `Cargo`-sized slots in heap memory. `parse_cargo_list` pre-counts the manifest lines so it can bolt exactly the right number of boxes to the wall — no more, no fewer (`cargo_capacity`). As items are filled in, `cargo_count` tracks how many boxes actually hold real cargo. Every loop that reads cargo must stop at `cargo_count`, never at `cargo_capacity`, and never one step beyond — because past the last filled box the clerk opens memory that belongs to someone else.

---

<svg viewBox="0 0 620 200" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Array layout and bounds in CargoForge-C</title>
  <desc>A row of memory slots labelled cargo[0] through cargo[N-1] shown as valid (teal), with cargo[N] shown in red as out-of-bounds, and arrows showing cargo_count stopping at the last valid slot.</desc>

  <!-- slot width=80, height=56, start x=20, y=72 -->
  <!-- slots 0..3 = valid -->
  <rect x="20"  y="72" width="80" height="56" fill="none" stroke="#12A594" stroke-width="2" rx="3"/>
  <rect x="100" y="72" width="80" height="56" fill="none" stroke="#12A594" stroke-width="2" rx="3"/>
  <rect x="180" y="72" width="80" height="56" fill="none" stroke="#12A594" stroke-width="2" rx="3"/>
  <rect x="260" y="72" width="80" height="56" fill="none" stroke="#12A594" stroke-width="2" rx="3"/>

  <!-- ellipsis gap -->
  <text x="358" y="105" text-anchor="middle" font-size="22" fill="currentColor" opacity="0.5">…</text>

  <!-- last valid slot -->
  <rect x="380" y="72" width="80" height="56" fill="none" stroke="#12A594" stroke-width="2" rx="3"/>

  <!-- out-of-bounds slot -->
  <rect x="460" y="72" width="80" height="56" fill="none" stroke="#D05663" stroke-width="2" stroke-dasharray="6 3" rx="3"/>

  <!-- slot labels -->
  <text x="60"  y="98"  text-anchor="middle" font-size="11" fill="#12A594" font-weight="600">cargo[0]</text>
  <text x="140" y="98"  text-anchor="middle" font-size="11" fill="#12A594" font-weight="600">cargo[1]</text>
  <text x="220" y="98"  text-anchor="middle" font-size="11" fill="#12A594" font-weight="600">cargo[2]</text>
  <text x="300" y="98"  text-anchor="middle" font-size="11" fill="#12A594" font-weight="600">cargo[3]</text>
  <text x="420" y="98"  text-anchor="middle" font-size="11" fill="#12A594" font-weight="600">cargo[N-1]</text>
  <text x="500" y="98"  text-anchor="middle" font-size="11" fill="#D05663" font-weight="600">cargo[N]</text>

  <!-- sub-labels -->
  <text x="60"  y="114" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">Cargo struct</text>
  <text x="140" y="114" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">Cargo struct</text>
  <text x="220" y="114" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">Cargo struct</text>
  <text x="300" y="114" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">Cargo struct</text>
  <text x="420" y="114" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">Cargo struct</text>
  <text x="500" y="114" text-anchor="middle" font-size="9" fill="#D05663" opacity="0.8">⛔ UB</text>

  <!-- cargo_count arrow -->
  <line x1="420" y1="60" x2="420" y2="70" stroke="#12A594" stroke-width="1.5" marker-end="url(#arr-teal)"/>
  <text x="420" y="52" text-anchor="middle" font-size="10" fill="#12A594" font-weight="600">cargo_count = N</text>
  <text x="420" y="40" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">(loop stops here)</text>

  <!-- cargo_capacity arrow -->
  <line x1="500" y1="60" x2="500" y2="70" stroke="#D05663" stroke-width="1.5" marker-end="url(#arr-red)"/>
  <text x="500" y="52" text-anchor="middle" font-size="10" fill="#D05663" font-weight="600">cargo_capacity = N</text>
  <text x="500" y="40" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">(do NOT index here)</text>

  <!-- legend label at bottom -->
  <text x="60"  y="148" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.5">heap base →</text>
  <text x="310" y="170" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.6">malloc(N × sizeof(Cargo)) — allocated by parse_cargo_list</text>

  <defs>
    <marker id="arr-teal" markerWidth="7" markerHeight="7" refX="4" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="#12A594"/>
    </marker>
    <marker id="arr-red" markerWidth="7" markerHeight="7" refX="4" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="#D05663"/>
    </marker>
  </defs>
</svg>

## What an array actually is

In C an array is a contiguous block of identically-sized objects in memory.
If you have:

```c
int ages[4] = {22, 35, 41, 19};
```

the runtime places four `int` values back-to-back in memory.
`ages[0]` sits at some address, say `0x1000`; `ages[1]` is at `0x1004`
(assuming a 4-byte `int`); `ages[2]` at `0x1008`; and so on.

The expression `ages[i]` is compiled to exactly `*(ages + i)` — take the base
address, add `i` element-widths, dereference. The CPU does no bounds check.
`ages[4]` compiles, links, and runs. It just reads four bytes past the end of
your array, returning whatever happened to be there — or, on a write, silently
corrupting whatever variable lived there.

!!! warning "The compiler will not save you"
    Writing `ages[4]` on the array above is **undefined behaviour** in C.
    It will not produce a compile error. AddressSanitizer will catch it at
    runtime; nothing else will, unless you're lucky enough to crash immediately.

---

## Fixed-size buffers — `char` arrays in `Cargo`

The simplest kind of array in CargoForge-C is a fixed-size character buffer
embedded directly inside a struct. From `cargoforge.h`, the `Cargo` struct
holds:

```c
char id[32];
char type[16];
```

These are not pointers. They live inside every `Cargo` object.
`id` can hold at most 31 printable characters plus a null terminator (`'\0'`).
Try to copy a 40-character identifier into it and you overflow into the next
field.

[`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) copies user-supplied strings into these buffers using the safe
idiom:

```c
strncpy(c->id, id, sizeof(c->id) - 1);
c->id[sizeof(c->id) - 1] = '\0';
```

Two details matter here:

1. `sizeof(c->id) - 1` caps the copy one byte short of the buffer, leaving
   room for the terminator.
2. The explicit `'\0'` write on the next line is belt-and-suspenders: if `id`
   is exactly 31 characters, `strncpy` fills all 31 bytes with content and
   does **not** write a terminator. The explicit assignment guarantees the
   string is always terminated.

This pattern appears for both `id` and `type` in `parse_cargo_list`.

---

## Dynamic arrays — allocating on the heap

Fixed-size buffers work when you know the maximum count at compile time.
A cargo manifest does not have a fixed length — it could hold 3 items or
3 000. The solution is a heap-allocated array: allocate exactly as many
elements as you need, store the pointer in the struct.

`Ship` (from `cargoforge.h`) uses:

```c
Cargo *cargo;          /* heap-allocated array */
int    cargo_count;    /* how many are populated */
int    cargo_capacity; /* how many slots were allocated */
```

`cargo` is a pointer. After `malloc` it points to a contiguous block of
`Cargo` structs in heap memory. `cargo[i]` is still just `*(cargo + i)` —
the same arithmetic as a stack array, but the memory came from the allocator.

---

## The count-then-allocate pattern

When reading from a file you can ask: "how many lines does this file have?"
before allocating. That is the two-pass approach in `parse_cargo_list`
(`src/parser.c:273–288`):

```c
/* Pass 1: count non-comment, non-blank lines */
int count = 0;
char line[MAX_LINE_LENGTH];
while (fgets(line, sizeof(line), file)) {
    if (line[0] != '#' && line[0] != '\n') count++;
}

/* Allocate exactly enough */
ship->cargo = malloc(count * sizeof(Cargo));
if (!ship->cargo) {
    fprintf(stderr, "Error: Failed to allocate memory for cargo.\n");
    fclose(file);
    return -1;
}
ship->cargo_capacity = count;
ship->cargo_count = 0;
rewind(file);
/* Pass 2: populate */
```

Why two passes instead of one? Because `malloc` needs a size before you
start. You could guess a large upper bound, but that wastes memory and
still leaves you managing a capacity ceiling. Counting first is free — a
file seek is cheap — and gives you an exact allocation.

The struct separates `cargo_capacity` (slots allocated) from `cargo_count`
(slots populated). Only `cargo_count` items are valid. Analysis code checks
`ship->cargo_count` to know how far to iterate; it never uses `cargo_capacity`
for logic.

!!! note "stdin cannot be rewound"
    The file-rewind trick requires a seekable file descriptor. `stdin` is a
    pipe — it cannot be rewound. For that case `parse_cargo_list` uses a
    different strategy: it reads every line into a dynamically growing
    `char **lines` buffer first (doubling capacity with `realloc` when full),
    then allocates `ship->cargo` with the final line count. Same logical
    result, one pass only.

---

## Off-by-one errors — the classic boundary mistake

Off-by-one errors arise whenever you confuse "number of elements" with
"last valid index". A few reference points to memorise:

| Expression | What it means |
|---|---|
| `arr[0]` | First element — always valid |
| `arr[n-1]` | Last element of an `n`-element array |
| `arr[n]` | One past the end — **undefined behaviour on read or write** |
| `i < n` | Safe loop condition |
| `i <= n` | Reads one past the end on the final iteration |

In `parse_cargo_list` the loop that reads dimensions uses a named constant
`MAX_DIMENSION` (defined as 3, for length/width/height):

```c
for (int d = 0; d < MAX_DIMENSION; ++d) {
    if (!tok) { dims_ok = false; break; }
    float dv = safe_atof(tok, 0.1f, 1e4f, "dimension");
    if (isnan(dv)) { dims_ok = false; break; }
    c->dimensions[d] = dv;
    tok = strtok_r(NULL, "x", &dim_saveptr);
}
```

`d` runs 0, 1, 2 — exactly the three valid indices of `dimensions[3]`.
The condition `d < MAX_DIMENSION` (not `d <= MAX_DIMENSION`) is what keeps
`dimensions[3]` unreachable. This is the standard pattern: use `<`, not `<=`,
when iterating over an array of size `N`.

---

## Growing a buffer with `realloc`

For stdin, the line buffer must grow as new lines arrive because the total
count is unknown. The code starts with capacity 100 and doubles when full
(`src/parser.c:241–250`):

```c
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
```

Three things to notice:

1. **Doubling** is not arbitrary — amortised analysis shows that doubling
   capacity on each resize keeps the average cost of inserting one element
   at $O(1)$ even though individual resizes cost $O(n)$.
2. **`realloc` returns a new pointer.** The original `lines` pointer may be
   invalid after the call. The code assigns to `new_lines` first and checks
   for NULL before touching `lines` — if it assigned directly to `lines` and
   `realloc` returned NULL, the original pointer would be lost and the already-
   allocated memory would leak forever.
3. **Cleanup on failure.** If `realloc` fails, the code frees each
   individually-`strdup`d line, then frees the pointer array itself, then
   returns -1. Forgetting any of those three steps produces a memory leak.

---

## Why bounds matter: what goes wrong when they are violated

Consider what lies immediately after `ship->cargo` in memory.
Nothing in particular — heap memory is managed by the allocator. An
out-of-bounds write into `ship->cargo[count]` or beyond might:

- Corrupt another live allocation (silent data corruption; symptoms appear
  somewhere completely unrelated).
- Corrupt allocator bookkeeping (crash inside `free` or `malloc`, with a
  confusing error message pointing at the allocator, not the real bug).
- Corrupt a pointer, causing a crash the next time anything is dereferenced.

None of these outcomes mention the line of code that caused the problem.
Bounds violations are among the hardest bugs to diagnose without a sanitizer,
because the symptom is usually far removed from the cause.

!!! example "AddressSanitizer in CargoForge-C"
    Running `make test-asan` rebuilds every test binary with
    `-fsanitize=address,undefined`. If any code reads or writes one byte
    past an array boundary, AddressSanitizer prints a precise report
    including the exact line number, the allocation site, and a stack trace.
    This is the tool the project used to find the heap-use-after-free bug
    in `parse_cargo_list` (see Lesson 13 for the full story).

---

## The dangling-pointer discipline

After `free(ship->cargo)`, the pointer `ship->cargo` still holds its old
numeric value. Any code that later dereferences it is reading freed memory —
a **use-after-free**. `parse_cargo_list` guards against this with an explicit
NULL-out on every error path (`src/parser.c:334–337`):

```c
for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
free(ship->cargo);
ship->cargo = NULL;    /* prevent use-after-free in ship_cleanup */
ship->cargo_count = 0; /* prevent iteration over freed memory */
```

Setting `ship->cargo = NULL` makes any accidental later dereference crash
immediately and predictably on a null pointer, rather than silently operating
on freed (and potentially reallocated) memory. Setting `ship->cargo_count = 0`
means any cleanup loop that iterates `for (i = 0; i < cargo_count; i++)` will
execute zero iterations — a safe no-op.

The same pair of assignments appears in the dimensions-parse error path
(`src/parser.c:362–365`). Every error path that frees the array must do both.
Missing either one is a bug.

---

## Recap

- A C array is a contiguous block of same-type objects; indexing is pointer
  arithmetic with no runtime bounds check.
- Fixed-size character buffers (`char id[32]`) use `strncpy` with `size - 1`
  and an explicit null terminator to prevent overflow.
- Dynamic arrays pair a pointer (`Cargo *cargo`) with a separate count
  (`cargo_count`) and capacity (`cargo_capacity`); only indices `[0, count)`
  are valid.
- The count-then-allocate (two-pass) pattern in `parse_cargo_list` determines
  the exact allocation size before touching heap memory.
- Always use `i < N` (not `i <= N`) when iterating over an N-element array.
- After `free(ptr)`, immediately set `ptr = NULL` and zero any associated count
  to prevent use-after-free on subsequent cleanup passes.

*Next: [Memory bugs (the real ones)](13-memory-bugs.md).*
