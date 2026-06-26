# Strings and char arrays

C has no built-in string type. Every piece of text — a cargo ID, a type tag, a UN number — is a raw array of bytes terminated by a special zero byte. Understanding this model is mandatory for reading CargoForge-C's parser, which processes untrusted text line by line and must never let a malformed manifest corrupt memory.

---

## What a string actually is in C

In C, a string is a `char` array whose last meaningful byte is the **NUL terminator** `'\0'` (ASCII value 0). Every standard library function that works on strings (`strlen`, `strcmp`, `strcpy`, `strncpy`, `strtok_r`) relies on finding this zero byte to know where the string ends.

```c
char id[32];   // room for 31 visible characters + one '\0'
```

The array occupies exactly 32 bytes in memory regardless of how much of it is used. If you store the five-character string `"CONT1"`, bytes 0–4 hold `'C','O','N','T','1'`, byte 5 holds `'\0'`, and bytes 6–31 are irrelevant (but still allocated).

```
index:  0    1    2    3    4    5    6  ... 31
value: 'C'  'O'  'N'  'T'  '1'  '\0'  ?  ...  ?
```

!!! warning "What happens without '\0'"
    If you fill all 32 bytes with non-zero characters and then call `strlen` or `printf("%s", ...)`, the function keeps reading past the array boundary, scanning the next variable on the stack (or heap) until it happens to find a zero byte. This is a **buffer over-read** — undefined behaviour that can leak data or crash the program.

---

## Fixed-size char arrays in the Cargo struct

The `Cargo` struct (defined in `cargoforge.h`) uses fixed-size arrays for the two text fields every cargo item carries:

| Field | Type | Max visible chars |
|---|---|---|
| `id` | `char[32]` | 31 |
| `type` | `char[16]` | 15 |

The sizes are chosen to be generous for realistic data (`"HeavyMachinery"` is 14 characters; `"hazardous"` is 9) while keeping the struct small enough to fit in a cache line alongside the float fields. Heap allocation would be wasteful for strings this short and would add a pointer-lifetime problem for every copy of the struct.

---

## Reading text from a line: `strtok_r`

The cargo manifest is whitespace-delimited:

```
HeavyMachinery    550    20x5x3    standard
FlammableLiquid    25    6x2.5x2.6    hazardous    DG:3.1:UN1203:A:F-E,S-D
```

`parse_cargo_list` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) tokenises each line with `strtok_r`:

```c
char *saveptr;
char *id     = strtok_r(line, " \t",   &saveptr);
char *w_str  = strtok_r(NULL, " \t",   &saveptr);
char *dim_str= strtok_r(NULL, " \t",   &saveptr);
char *type   = strtok_r(NULL, " \t\n", &saveptr);
```

### How `strtok_r` works

`strtok_r(str, delimiters, &saveptr)` scans `str` for the next token separated by any character in `delimiters`. On the first call you pass the string; on subsequent calls you pass `NULL` and the function resumes from where it left off, using `saveptr` to remember its position between calls.

Crucially, **`strtok_r` modifies the input buffer in place**: it replaces each delimiter it finds with a `'\0'`, so the returned pointer points directly into the original `line` array. No heap allocation happens. The `_r` suffix means "re-entrant" — `saveptr` is caller-supplied state, so two concurrent tokenisation loops (such as the outer manifest loop and the inner dimension loop) can run without interfering.

!!! note "Why not plain `strtok`?"
    The original `strtok` stores its position in a hidden global variable. The dimension parser inside the same function needs its own independent position (`dim_saveptr`). Using plain `strtok` for both would cause the outer loop to lose its place when the inner dimension loop runs. `strtok_r` avoids this by making the state explicit.

The dimension field `"20x5x3"` is tokenised with a second, nested `strtok_r` using `"x"` as delimiter:

```c
char *dim_saveptr;
char *tok = strtok_r(dim_str, "x", &dim_saveptr);
for (int d = 0; d < MAX_DIMENSION; ++d) {
    if (!tok) { dims_ok = false; break; }
    float dv = safe_atof(tok, 0.1f, 1e4f, "dimension");
    if (isnan(dv)) { dims_ok = false; break; }
    c->dimensions[d] = dv;
    tok = strtok_r(NULL, "x", &dim_saveptr);
}
```

Two independent `saveptr` variables keep the two tokenisation loops completely separate.

---

## Copying into fixed buffers: `strncpy` and the explicit guard

Once the tokens are extracted, they are copied into the `Cargo` struct's fixed arrays. The parser does this in two lines for each field (from [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)):

```c
strncpy(c->id,   id,   sizeof(c->id)   - 1);
c->id[sizeof(c->id)   - 1] = '\0';
strncpy(c->type, type, sizeof(c->type) - 1);
c->type[sizeof(c->type) - 1] = '\0';
```

### Why `strncpy`, not `strcpy`?

`strcpy(dst, src)` copies bytes until it hits `'\0'` in `src`, writing as many bytes as the source string requires — with no regard for how large `dst` is. If a user passes a 200-character cargo ID, `strcpy` silently overwrites the 168 bytes after `c->id`, corrupting whatever struct fields follow it in memory.

`strncpy(dst, src, n)` copies **at most `n` bytes**. The `-1` argument (`sizeof(c->id) - 1`) leaves room for the terminator. But there is a subtle trap: if `src` is exactly `n` bytes long with no `'\0'` in the first `n` characters, `strncpy` fills `dst` with `n` non-zero bytes and writes **no terminator**. The explicit assignment on the next line patches that case:

```c
c->id[sizeof(c->id) - 1] = '\0';
```

This forces a terminator into the very last byte of the array regardless of what `strncpy` did. The pattern is so common in C systems code that it is worth memorising as a unit:

```c
strncpy(dst, src, sizeof(dst) - 1);
dst[sizeof(dst) - 1] = '\0';
```

!!! tip "Use `sizeof`, not a literal"
    Writing `strncpy(c->id, id, 31)` is fragile — if the struct field is ever resized, the magic number silently goes stale. `sizeof(c->id) - 1` tracks the actual array size automatically.

---

## Parsing the DG field: a second `strtok_r` over a copy

The optional DG field in the manifest has its own colon-delimited grammar:

```
DG:3.1:UN1203:A:F-E,S-D
```

`parse_dg_field` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) handles this entirely within a 64-byte stack buffer so it never touches the caller's memory:

```c
static DGInfo *parse_dg_field(const char *field) {
    if (!field || strncmp(field, "DG:", 3) != 0)
        return NULL;

    DGInfo *dg = calloc(1, sizeof(DGInfo));
    if (!dg) return NULL;

    /* Work on a copy */
    char buf[64];
    strncpy(buf, field + 3, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr;
    char *tok = strtok_r(buf, ":", &saveptr);
    ...
```

Several design decisions are packed into those eight lines:

1. **`strncmp(field, "DG:", 3)`** — checks the three-character prefix without requiring the whole string to match. If the prefix is absent the function returns `NULL` immediately, so a non-DG manifest field causes no allocation.

2. **`field + 3`** — pointer arithmetic. `field` points to the start of `"DG:3.1:UN1203:..."`. Adding 3 advances the pointer past the prefix to `"3.1:UN1203:..."`. No copy of the prefix is needed.

3. **Copy into `buf` before calling `strtok_r`** — `strtok_r` mutates the buffer it works on (replacing `:` with `'\0'`). The original `field` pointer is `const char *` — writing to a `const` string is undefined behaviour and will crash if the string lives in read-only memory (as string literals often do). Copying to a local `buf` first gives `strtok_r` writable memory to modify.

4. **`buf[sizeof(buf) - 1] = '\0'`** — the explicit terminator guard after `strncpy`, for the same reason shown above.

The UN number and EmS string are then copied into `dg->un_number` and `dg->ems` with the same `strncpy` + explicit terminator pattern:

```c
tok = strtok_r(NULL, ":", &saveptr);
if (tok) {
    strncpy(dg->un_number, tok, sizeof(dg->un_number) - 1);
}
```

Note that no explicit terminator assignment appears here — `calloc` zeroed the entire `DGInfo` struct on allocation, so `dg->un_number` is already filled with `'\0'` bytes before `strncpy` runs. Both approaches (explicit assignment and rely-on-calloc) are correct; the explicit assignment is more defensive.

---

## Why C strings are dangerous: a mental model

Every bug involving C strings follows one of three shapes:

| Shape | What happens | Example trigger |
|---|---|---|
| **Over-read** | Reading past `'\0'` because no terminator was written | `strncpy` without explicit `'\0'` on a full buffer |
| **Over-write** | Writing past the end of the array | `strcpy` with a longer source |
| **Use-after-free** | Pointer to freed memory still read | Stale `char *` after `free(buf)` |

CargoForge-C's parser defends against all three:
- Fixed arrays + `strncpy` + explicit terminator → no over-read or over-write.
- Copy-to-stack-buffer before `strtok_r` → no mutation of `const` or shared memory.
- `ship->cargo = NULL` after `free` in error paths → no dangling pointer (see the bug journal entry in `docs/BUG_JOURNAL.md` for the exact heap-use-after-free this fixed).

---

## The ship-config path: `strncpy` for file paths

`parse_ship_config` uses the same pattern for optional file paths:

```c
char hydro_path[256] = {0};
char tanks_path[256] = {0};
...
if (strcmp(key, "hydrostatic_table") == 0) {
    strncpy(hydro_path, value, sizeof(hydro_path) - 1);
    continue;
}
```

The `= {0}` initialiser zero-fills the entire 256-byte array at declaration. This means `hydro_path[0] == '\0'` until a value is parsed, which lets the check `if (hydro_path[0] != '\0')` later in the function detect whether the key was actually present — a clean sentinel that avoids a separate boolean flag.

---

## Recap

- A C string is a `char` array terminated by `'\0'`. Standard library functions find the end by scanning for this byte; a missing terminator causes them to read past the array boundary.
- `Cargo` stores `id` (`char[32]`) and `type` (`char[16]`) as fixed-size arrays — enough for real data, small enough to live inline in the struct.
- `strncpy(dst, src, sizeof(dst) - 1)` followed by `dst[sizeof(dst) - 1] = '\0'` is the canonical safe copy idiom; the explicit assignment handles the case where `src` is exactly as long as the buffer.
- `strtok_r` tokenises a mutable buffer in place, using a caller-supplied `saveptr` so two independent tokenisation loops (manifest fields and dimension components) can run without interfering.
- Always copy a `const char *` to a local buffer before passing it to `strtok_r` — the function writes `'\0'` bytes into the buffer, and writing to a `const` or read-only string is undefined behaviour.
- Zero-initialising a char array with `= {0}` gives a free empty-string sentinel without a separate boolean flag.

*Next: [Error handling without exceptions](08-error-handling-without-exceptions.md).*
