# Tokenizing and parsing in C

CargoForge-C reads two kinds of text input: a ship configuration file (key=value pairs) and a
cargo manifest (whitespace-delimited rows). Both live in `src/parser.c`. This lesson walks
through exactly how the code turns a raw text line into a populated `Cargo` struct — using
`fgets`, `strtok_r`, and a carefully written `safe_atof` — and explains why each choice was
made the way it was.

---

## Reading lines with `fgets`

The standard library gives you two ways to read a line of text from a file: `gets` (dangerous,
removed in C11) and `fgets`. CargoForge-C uses only `fgets`. The call is:

```c
char line[MAX_LINE_LENGTH];
while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#' || line[0] == '\n') continue;
    /* process line */
}
```

`fgets(buffer, size, stream)` reads at most `size - 1` characters and always appends a `'\0'`
terminator, so the buffer never overflows. If the line is longer than the buffer, the rest is
left in the stream for the next call — you get a truncated line rather than a crash.

The two-character guard `line[0] == '#' || line[0] == '\n'` skips comment lines and blank
lines before any tokenisation work starts. This is the idiomatic C pattern for simple line
filters.

!!! note "stdin support"
    `parse_cargo_list` supports `filename = "-"` to read from stdin. Because `stdin` cannot be
    rewound, the function reads all lines into a heap-allocated `char **lines` array first, then
    processes that array exactly as it would a file. The two-pass strategy for files (count
    lines, allocate, re-read) becomes a single buffered pass for stdin. The loop body is
    otherwise identical.

---

## Why `strtok` is not enough — meet `strtok_r`

`strtok` is the classic tokeniser: it splits a string on delimiter characters and returns
successive tokens. Its fatal flaw is that it stores its position in a global (hidden) variable.
Call `strtok` twice — perhaps once in a loop and once inside a helper function — and the inner
call silently resets the outer one.

`strtok_r` (the POSIX re-entrant version) solves this by giving each call its own `char *saveptr`
to hold the position:

```c
char *saveptr;
char *id      = strtok_r(line,  " \t",   &saveptr);
char *w_str   = strtok_r(NULL,  " \t",   &saveptr);
char *dim_str = strtok_r(NULL,  " \t",   &saveptr);
char *type    = strtok_r(NULL,  " \t\n", &saveptr);
```

Passing `NULL` as the first argument after the first call tells `strtok_r` to continue from
where it left off — using `saveptr` rather than a global. When `parse_cargo_list` later needs
to tokenise the dimension string `"20x5x3"` on its own inner delimiter `'x'`, it uses a
*separate* `dim_saveptr`:

```c
char *dim_saveptr;
char *tok = strtok_r(dim_str, "x", &dim_saveptr);
for (int d = 0; d < MAX_DIMENSION; ++d) {
    /* ... */
    tok = strtok_r(NULL, "x", &dim_saveptr);
}
```

Because `dim_saveptr` is distinct from `saveptr`, the outer loop's position is untouched.
This nested tokenisation would be impossible with plain `strtok`.

!!! warning "strtok modifies the string"
    Both `strtok` and `strtok_r` write `'\0'` bytes into the buffer to terminate each token.
    After the first call, `line` is no longer a single contiguous string — it is a series of
    null-terminated fragments. Never pass a string literal to `strtok_r`; always work on a
    mutable copy or an array (as `fgets` always provides).

---

## Converting text to numbers safely with `safe_atof`

The C standard library offers `atof` for string-to-float conversion. It is convenient and
wrong for production use: it returns 0.0 on error, which is indistinguishable from a legitimately
zero input, and it provides no range checking.

`parser.c` defines a private helper that fixes both problems:

```c
static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    errno = 0;
    float val = strtof(s, &end);

    if (errno != 0 || end == s || (*end != '\0' && *end != '\n')
        || val < min || val > max) {
        fprintf(stderr, "Error: Invalid or out-of-range %s value '%s'\n",
                field_name, s);
        return NAN;
    }
    return val;
}
```

The four conditions checked are:

| Condition | What it catches |
|-----------|-----------------|
| `errno != 0` | Overflow or underflow flagged by `strtof` |
| `end == s` | No digits were consumed at all (e.g., `"abc"`) |
| `*end != '\0' && *end != '\n'` | Trailing garbage after the number (e.g., `"5.0xyz"`) |
| `val < min \|\| val > max` | Out-of-range for this specific field |

On any failure the function returns `NAN` (the IEEE 754 "not a number" sentinel) and prints a
diagnostic. Callers test with `isnan(v)` and abort cleanly:

```c
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    /* clean up and return -1 */
}
```

Using NAN as an error sentinel is idiomatic in C numeric code because it propagates through
arithmetic (any expression involving NAN produces NAN) and is easy to test for. It also avoids
the "0 looks valid" trap of `atof`.

---

## Building a `Cargo` from one line

A cargo manifest line looks like:

```
FlammableLiquid   25   6x2.5x2.6   hazardous   DG:3.1:UN1203:A:F-E,S-D
```

`parse_cargo_list` turns this into a `Cargo` struct through a precise sequence of steps
(from `src/parser.c`):

### Step 1 — tokenise the five fields

```c
char *saveptr;
char *id      = strtok_r(line,  " \t",   &saveptr);
char *w_str   = strtok_r(NULL,  " \t",   &saveptr);
char *dim_str = strtok_r(NULL,  " \t",   &saveptr);
char *type    = strtok_r(NULL,  " \t\n", &saveptr);
char *dg_field = strtok_r(NULL, " \t\n", &saveptr);
```

The fifth token `dg_field` is optional — `strtok_r` returns `NULL` if no more tokens exist,
and the caller checks for NULL before using it.

### Step 2 — initialise the struct to a known state

```c
Cargo *c = &ship->cargo[ship->cargo_count];
memset(c, 0, sizeof(*c));
strncpy(c->id, id, sizeof(c->id) - 1);
c->id[sizeof(c->id) - 1] = '\0';
strncpy(c->type, type, sizeof(c->type) - 1);
c->type[sizeof(c->type) - 1] = '\0';
c->pos_x = -1.0f;
c->pos_y = -1.0f;
c->pos_z = -1.0f;
c->dg = NULL;
```

`memset` zeroes every byte of the struct before any fields are written. This prevents
unintialised-read bugs if a new field is added later but a parsing path forgets to set it.
The three position fields are set to `-1.0f` as a sentinel meaning "not yet placed" — analysis
code everywhere uses `pos_x < 0` to skip unplaced items.

The `strncpy` pattern copies at most `sizeof(c->id) - 1` bytes and then explicitly writes a
`'\0'` at the last position. This guarantees the `id` array is always null-terminated even if
the source string fills the buffer exactly. (`strncpy` does *not* guarantee termination when
the source is at least as long as the size argument.)

### Step 3 — parse and convert weight

```c
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;
    ship->cargo_count = 0;
    /* ... close file, free lines buffer ... */
    return -1;
}
c->weight = weight_t * 1000.0f;   /* tonnes → kg */
```

The manifest records weight in tonnes; the `Cargo` struct stores kilograms. The multiplication
happens at the parse boundary so that everything downstream works in consistent SI units.

The error path frees every DG pointer allocated so far, frees the cargo array, and critically
sets `ship->cargo = NULL` and `ship->cargo_count = 0` before returning. Without those two
assignments, `ship_cleanup` would later iterate a freed array — a heap-use-after-free (the
exact bug the fuzzer found and that was fixed in this codebase).

### Step 4 — parse dimensions

```c
char *dim_saveptr;
char *tok = strtok_r(dim_str, "x", &dim_saveptr);
bool dims_ok = true;
for (int d = 0; d < MAX_DIMENSION; ++d) {
    if (!tok) { dims_ok = false; break; }
    float dv = safe_atof(tok, 0.1f, 1e4f, "dimension");
    if (isnan(dv)) { dims_ok = false; break; }
    c->dimensions[d] = dv;
    tok = strtok_r(NULL, "x", &dim_saveptr);
}
```

The dimension string `"6x2.5x2.6"` is split on `'x'` with its own `dim_saveptr`, filling
`c->dimensions[0]` (length), `c->dimensions[1]` (width), and `c->dimensions[2]` (height) in
metres. `MAX_DIMENSION` is 3. If any component is absent or invalid, `dims_ok` is set false
and the same cleanup-and-abort pattern as the weight path runs.

### Step 5 — parse the optional DG field

```c
if (dg_field) {
    c->dg = (struct DGInfo_ *)parse_dg_field(dg_field);
}
```

`parse_dg_field` expects the grammar `DG:<class>[.<division>]:<UN>:<stowage>:<EmS>`. It uses
`strtok_r` on a 64-byte working copy of the field (skipping the `"DG:"` prefix), splitting on
`':'` to extract each sub-field. The function heap-allocates a `DGInfo` struct, validates that
`dg_class` is in [1, 9], and returns NULL if the prefix is absent or the class is invalid.
`c->dg` remains `NULL` for all non-DG cargo — the placement and IMDG engines check for NULL
before dereferencing it.

### Step 6 — commit the item

```c
ship->cargo_count++;
```

Only incremented after every field has been successfully parsed and stored. If the function
had incremented first and then encountered a parse error, the cleanup loop would try to free
`c->dg` on the partially-initialised struct just written — a subtle class of bug this ordering
prevents.

---

## The ship config parser — a simpler design

`parse_ship_config` uses a slightly different tokenising strategy because its format is
`key=value` rather than whitespace-delimited columns. It uses plain `strtok` (not `strtok_r`)
because the loop body never calls a nested tokeniser:

```c
char line[MAX_LINE_LENGTH];
while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#' || line[0] == '\n') continue;

    char *key   = strtok(line, "=");
    char *value = strtok(NULL, "\n");
    if (!key || !value) continue;

    while (*value == ' ' || *value == '\t') value++;  /* trim leading whitespace */

    float v = safe_atof(value, 0.1f, 1e9f, key);
    if (isnan(v)) { /* ... */ return -1; }

    if (strcmp(key, "length_m") == 0)          ship->length = v;
    else if (strcmp(key, "max_weight_tonnes") == 0) ship->max_weight = v * 1000.0f;
    /* ... further keys ... */
}
```

String-valued keys (`hydrostatic_table`, `tank_config`) bypass `safe_atof` entirely and are
copied into a local path buffer with `strncpy`, then loaded after the loop completes. This
defers file I/O (opening the hydrostatics CSV or tank CSV) until the entire config has been
read, so a missing path never leaves the ship half-configured.

---

## How the pieces fit together

The parsing layer sits at the entry point to every CargoForge-C run. `main.c` calls
`parse_ship_config` and `parse_cargo_list`, then passes the populated `Ship` struct to
`perform_analysis` or `place_cargo_3d`. Nothing downstream re-reads the files.

```
text files
    │
    ├── parse_ship_config()   →  Ship.length, .width, .max_weight, .hydro, .tanks, …
    │       fgets + strtok + safe_atof
    │
    └── parse_cargo_list()    →  Ship.cargo[], Ship.cargo_count
            fgets + strtok_r + safe_atof + parse_dg_field
                                     │
                                     └── DGInfo (heap-allocated, owned by Cargo.dg)
```

All numeric conversion happens at this boundary. All sentinel values (`-1.0f` for positions,
`NULL` for optional pointers) are established here. Everything downstream can trust that if
`parse_cargo_list` returned 0, every `Cargo` in the array is fully initialised with valid
data.

---

## Recap

- `fgets(buf, size, stream)` reads at most `size - 1` bytes and always null-terminates —
  use it instead of `gets` for any line-by-line reading.
- `strtok_r` is the re-entrant replacement for `strtok`; its `saveptr` argument makes nested
  tokenisation safe by keeping state per call-site rather than in a hidden global.
- `safe_atof` wraps `strtof` with range checking and returns `NAN` on error; callers test
  with `isnan` and abort cleanly — avoiding the silent-zero trap of `atof`.
- Weight values are converted from tonnes to kilograms at the parse boundary (`× 1000.0f`) so
  that all analysis code works in consistent SI units without repeated conversions.
- Unplaced `Cargo` items carry `pos_x = -1.0f` as a sentinel; analysis and placement code
  checks `pos_x < 0` to skip them.
- On any parse error, both the cargo array pointer and its count must be zeroed before
  returning — leaving a stale non-NULL pointer is the root cause of the heap-use-after-free
  bug the fuzzer caught.

*Next: [Validation and robustness](28-validation-and-robustness.md).*
