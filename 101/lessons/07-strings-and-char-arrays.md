# Strings and char arrays

C has no built-in string type. Every piece of text — a cargo ID, a type tag, a UN number — is a raw array of bytes terminated by a special zero byte. Understanding this model is mandatory for reading CargoForge-C's parser, which processes untrusted text line by line and must never let a malformed manifest corrupt memory.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **NUL terminator (`'\0'`)** = "the stop sign at the end of every string" — it's the single zero byte that tells `strlen`, `printf`, and every other C string function where your text ends; without it, they keep reading into whatever memory comes next.
- **Fixed-size char array (`char id[32]`)** = "a reserved parking lot with exactly 32 spaces" — `Cargo` uses these for `id` and `type` so the text lives right inside the struct without a separate heap allocation; 31 usable characters is enough for any realistic cargo label.
- **`strncpy` + explicit terminator** = "copy at most N bytes, then force-write the stop sign yourself" — `parse_cargo_list` always follows `strncpy(c->id, id, sizeof(c->id) - 1)` with `c->id[sizeof(c->id) - 1] = '\0'` because `strncpy` won't write the terminator if the source fills the buffer exactly.
- **`strtok_r`** = "a whitespace splitter that remembers where it left off, per caller" — `parse_cargo_list` uses two independent `saveptr` variables so the outer manifest-field loop and the inner `"x"`-delimited dimension loop can both run at the same time without stepping on each other.
- **Copy before `strtok_r`** = "give the slicer its own scratch copy so it doesn't destroy read-only memory" — `parse_dg_field` copies the DG field into a local `buf[64]` before tokenising because `strtok_r` writes `'\0'` bytes into the buffer it works on, and mutating a `const char *` is undefined behaviour.
- **Use-after-free** = "reading from an address after you've already handed that memory back" — one of the three classic C-string bug shapes; CargoForge-C defends against it by setting `ship->cargo = NULL` immediately after `free`, so a stale pointer can't silently re-read freed data (documented in `docs/BUG_JOURNAL.md`).

**Why it matters:** every C-string bug — buffer overrun, missing terminator, stale pointer — is invisible at compile time and silently corrupts memory at runtime, so a single malformed manifest line could corrupt the ship's `Cargo` array or leak the bytes of a neighbouring struct field; the `strncpy`-plus-explicit-terminator pattern and the copy-before-`strtok_r` discipline are what keep `parse_cargo_list` and `parse_dg_field` safe against untrusted input.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> A C string is a **parking lot with a red cone at the end of the last occupied space.** The lot (`char id[32]`) always has exactly 32 spaces. You park five cars (`'C','O','N','T','1'`), then drop a red cone (`'\0'`) in space 6. Every function (`strlen`, `printf`, `strncpy`) drives through the lot and stops the moment it sees the cone — it never looks at the empty spaces beyond. Remove the cone and the function drives off the edge of the lot into whatever is parked next door.

In CargoForge-C the parking lot is `c->id` (32 spaces) or `c->type` (16 spaces), carved right into the `Cargo` struct. `strncpy` fills the spaces, but if a cargo ID arrives exactly 31 characters long with no room left for a cone, `strncpy` parks the last car and walks away. The explicit `c->id[sizeof(c->id) - 1] = '\0'` is you personally dropping the cone into the last space before anyone else drives through. `strtok_r` is the valet who needs a scratch copy (`buf[64]`) because it pulls cones out and jams them between tokens — you never hand it the original lot.

<svg viewBox="0 0 620 210" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>C string memory layout for char id[32]</title>
  <desc>A row of 32 memory cells representing char id[32]. The first five cells contain C, O, N, T, 1. Cell index 5 is highlighted in teal and contains the NUL terminator. The remaining cells are shown dimmed and labelled unused.</desc>

  <!-- row label -->
  <text x="10" y="52" font-size="12" fill="currentColor" opacity="0.6" font-style="italic">char id[32]</text>

  <!-- cells 0-5 (visible chars + NUL) -->
  <!-- index labels -->
  <text x="68"  y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">0</text>
  <text x="108" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">1</text>
  <text x="148" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">2</text>
  <text x="188" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">3</text>
  <text x="228" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">4</text>
  <text x="268" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">5</text>
  <text x="316" y="26" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.5">6 … 31</text>

  <!-- character cells -->
  <rect x="48"  y="34" width="40" height="40" rx="4" fill="none" stroke="currentColor" stroke-width="1.4"/>
  <rect x="88"  y="34" width="40" height="40" rx="4" fill="none" stroke="currentColor" stroke-width="1.4"/>
  <rect x="128" y="34" width="40" height="40" rx="4" fill="none" stroke="currentColor" stroke-width="1.4"/>
  <rect x="168" y="34" width="40" height="40" rx="4" fill="none" stroke="currentColor" stroke-width="1.4"/>
  <rect x="208" y="34" width="40" height="40" rx="4" fill="none" stroke="currentColor" stroke-width="1.4"/>

  <!-- NUL cell — teal highlight -->
  <rect x="248" y="34" width="40" height="40" rx="4" fill="#12A594" fill-opacity="0.18" stroke="#12A594" stroke-width="2"/>

  <!-- unused cells block -->
  <rect x="296" y="34" width="80" height="40" rx="4" fill="none" stroke="currentColor" stroke-width="1" stroke-dasharray="4 3" opacity="0.35"/>

  <!-- character text -->
  <text x="68"  y="60" font-size="15" text-anchor="middle" fill="currentColor" font-weight="600">'C'</text>
  <text x="108" y="60" font-size="15" text-anchor="middle" fill="currentColor" font-weight="600">'O'</text>
  <text x="148" y="60" font-size="15" text-anchor="middle" fill="currentColor" font-weight="600">'N'</text>
  <text x="188" y="60" font-size="15" text-anchor="middle" fill="currentColor" font-weight="600">'T'</text>
  <text x="228" y="60" font-size="15" text-anchor="middle" fill="currentColor" font-weight="600">'1'</text>
  <text x="268" y="55" font-size="13" text-anchor="middle" fill="#12A594" font-weight="700">'\0'</text>
  <text x="336" y="58" font-size="11" text-anchor="middle" fill="currentColor" opacity="0.35">unused</text>

  <!-- NUL label -->
  <text x="268" y="90" font-size="10" text-anchor="middle" fill="#12A594">NUL terminator</text>
  <line x1="268" y1="74" x2="268" y2="84" stroke="#12A594" stroke-width="1.2"/>

  <!-- strncpy arrow + label -->
  <line x1="48" y1="118" x2="246" y2="118" stroke="currentColor" stroke-width="1.2" stroke-dasharray="3 2" opacity="0.5"/>
  <polygon points="246,114 254,118 246,122" fill="currentColor" opacity="0.5"/>
  <text x="145" y="112" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.6">strncpy copies up to sizeof(id)−1 bytes</text>

  <!-- explicit guard arrow -->
  <line x1="268" y1="130" x2="268" y2="148" stroke="#D05663" stroke-width="1.4"/>
  <polygon points="264,148 272,148 268,155" fill="#D05663"/>
  <text x="370" y="143" font-size="10" fill="#D05663">c->id[sizeof(c->id)−1] = '\0'</text>
  <text x="370" y="157" font-size="10" fill="#D05663">forces the stop sign</text>

  <!-- danger label if NUL missing -->
  <rect x="48" y="168" width="330" height="28" rx="4" fill="#D05663" fill-opacity="0.10" stroke="#D05663" stroke-width="1" stroke-dasharray="3 2"/>
  <text x="213" y="187" font-size="10" text-anchor="middle" fill="#D05663">Without '\0': strlen/printf reads past index 31 → buffer over-read</text>
</svg>

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
