# Tokenizing and parsing in C

CargoForge-C reads two kinds of text input: a ship configuration file (key=value pairs) and a
cargo manifest (whitespace-delimited rows). Both live in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c). This lesson walks
through exactly how the code turns a raw text line into a populated `Cargo` struct — using
`fgets`, `strtok_r`, and a carefully written `safe_atof` — and explains why each choice was
made the way it was.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **`fgets`** = "read one line at a time, safely" — unlike the banned `gets`, `fgets` takes a size argument so it can never write more bytes than your buffer holds; every `while (fgets(line, sizeof(line), file))` loop in `parse_cargo_list` and `parse_ship_config` relies on this guarantee.
- **Tokenising** = "chopping a line of text into labelled pieces" — a raw manifest line like `FlammableLiquid 25 6x2.5x2.6 hazardous DG:3.1:UN1203:A:F-E,S-D` is just one long string until `strtok_r` splits it on spaces and tabs into the five fields that fill a `Cargo` struct.
- **`strtok_r` vs `strtok`** = "thread-safe tokeniser that remembers where it is without using a hidden global" — `parse_cargo_list` needs to split a line on spaces *and then* split the dimension sub-string `"6x2.5x2.6"` on `'x'`; plain `strtok` would lose its place during the inner split, so `strtok_r` is used with two separate `saveptr` variables (`saveptr` and `dim_saveptr`) to keep the two loops independent.
- **`safe_atof`** = "convert text to a number and refuse to silently accept garbage" — the standard `atof` returns 0.0 for both `"0.0"` and `"abc"`, making errors invisible; `safe_atof` wraps `strtof`, checks four failure conditions, and returns `NAN` so callers can test `isnan(v)` and abort cleanly.
- **`NAN` as an error sentinel** = "a special floating-point value that says 'something went wrong'" — because `NAN` propagates through arithmetic and is easy to test for with `isnan()`, it avoids the "zero looks valid" trap; every bad field in the manifest triggers a NAN return and a controlled teardown in `parse_cargo_list`.
- **`memset` + sentinel positions** = "start from a known zero state, then mark fields that haven't been set yet" — each new `Cargo` is zeroed with `memset` before any field is written, and `pos_x`, `pos_y`, `pos_z` are set to `-1.0f` so that `perform_analysis` and `place_cargo_3d` can safely skip unplaced items by checking `pos_x < 0`.
- **Zeroing the pointer and count on error** = "prevent the cleanup code from chasing a freed array" — the heap-use-after-free bug the fuzzer found happened because a failed parse returned without setting `ship->cargo = NULL` and `ship->cargo_count = 0`; `ship_cleanup` then iterated the already-freed array; the fix is to null both fields before returning `-1`.

**Why it matters:** a parser that silently accepts bad input — wrong units, out-of-range weights, half-written manifests — corrupts the `Ship` struct that every downstream calculation (`perform_analysis`, `place_cargo_3d`, GM and stability checks) trusts completely; one missed null-pointer or stale array pointer turns a parse error into a crash or, worse, a silent wrong answer on a loaded vessel.

---

## The mental model 🧠

You'll forget the function names — hold THIS picture instead:

> A port customs officer receives a shipping container label — one long strip of text.
> She tears it at every space into five paper slips (id, weight, dimensions, type, DG code),
> hands each slip to a specialist clerk, and only stamps the cargo "cleared" when every clerk
> gives a thumbs-up. If any slip is unreadable, she shreds all five and marks the entry void.

That's `parse_cargo_list` exactly. The label is the raw manifest line. Tearing is `strtok_r` splitting on spaces and storing the result in `saveptr`. The specialist clerks are `safe_atof` (who rejects "5.0xyz" and returns `NAN`) and the inner `dim_saveptr` loop (who tears `"6x2.5x2.6"` again on `'x'`). The "cleared" stamp is `ship->cargo_count++` — only incremented after every field passes. Shredding means freeing every `c->dg` pointer, nulling `ship->cargo`, and zeroing `ship->cargo_count` before returning `-1`. The customs table starts blank (`memset`) and the position fields read `-1.0f` until placement code fills them in.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Tokenizing and parsing pipeline in parse_cargo_list</title>
  <desc>A left-to-right flow showing a raw manifest line being split by strtok_r into five tokens, validated by safe_atof and parse_dg_field, then assembled into a Cargo struct.</desc>

  <!-- Raw line input box -->
  <rect x="8" y="120" width="148" height="42" rx="4"
        fill="none" stroke="currentColor" stroke-width="1.4" opacity="0.55"/>
  <text x="82" y="137" text-anchor="middle" font-size="10.5" fill="currentColor" opacity="0.75">Raw manifest line</text>
  <text x="82" y="153" text-anchor="middle" font-size="9" fill="#12A594" font-family="monospace">fgets( line, … )</text>

  <!-- Arrow: raw line → strtok_r -->
  <line x1="156" y1="141" x2="184" y2="141" stroke="currentColor" stroke-width="1.3" opacity="0.55" marker-end="url(#arr)"/>

  <!-- strtok_r box -->
  <rect x="184" y="108" width="108" height="66" rx="4"
        fill="none" stroke="#12A594" stroke-width="1.6"/>
  <text x="238" y="127" text-anchor="middle" font-size="10.5" fill="#12A594">strtok_r</text>
  <text x="238" y="142" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">splits on spaces</text>
  <text x="238" y="155" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">stores in saveptr</text>
  <text x="238" y="168" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">+ dim_saveptr</text>

  <!-- Five token outputs from strtok_r -->
  <!-- token arrows go to validation column -->
  <!-- id -->
  <line x1="292" y1="124" x2="318" y2="72" stroke="currentColor" stroke-width="1.1" opacity="0.45" marker-end="url(#arr)"/>
  <rect x="318" y="52" width="76" height="26" rx="3" fill="none" stroke="currentColor" stroke-width="1.1" opacity="0.55"/>
  <text x="356" y="69" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.8">id → strncpy</text>

  <!-- weight -->
  <line x1="292" y1="133" x2="318" y2="113" stroke="currentColor" stroke-width="1.1" opacity="0.45" marker-end="url(#arr)"/>
  <rect x="318" y="93" width="96" height="26" rx="3" fill="none" stroke="currentColor" stroke-width="1.1" opacity="0.55"/>
  <text x="366" y="110" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.8">weight → safe_atof</text>

  <!-- dims -->
  <line x1="292" y1="141" x2="318" y2="141" stroke="currentColor" stroke-width="1.1" opacity="0.45" marker-end="url(#arr)"/>
  <rect x="318" y="128" width="108" height="26" rx="3" fill="none" stroke="#12A594" stroke-width="1.3"/>
  <text x="372" y="141" text-anchor="middle" font-size="9.5" fill="#12A594">6x2.5x2.6 → dim_saveptr</text>
  <text x="372" y="151" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.65">safe_atof × 3</text>

  <!-- type -->
  <line x1="292" y1="152" x2="318" y2="174" stroke="currentColor" stroke-width="1.1" opacity="0.45" marker-end="url(#arr)"/>
  <rect x="318" y="162" width="76" height="26" rx="3" fill="none" stroke="currentColor" stroke-width="1.1" opacity="0.55"/>
  <text x="356" y="179" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.8">type → strncpy</text>

  <!-- dg field -->
  <line x1="292" y1="163" x2="318" y2="207" stroke="currentColor" stroke-width="1.1" opacity="0.45" marker-end="url(#arr)"/>
  <rect x="318" y="196" width="108" height="26" rx="3" fill="none" stroke="currentColor" stroke-width="1.1" opacity="0.55"/>
  <text x="372" y="213" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.8">DG field → parse_dg_field</text>

  <!-- Cargo struct assembly box -->
  <rect x="450" y="56" width="116" height="192" rx="5"
        fill="none" stroke="#12A594" stroke-width="1.7"/>
  <text x="508" y="76" text-anchor="middle" font-size="11" fill="#12A594" font-weight="600">Cargo struct</text>
  <line x1="458" y1="82" x2="558" y2="82" stroke="currentColor" stroke-width="0.8" opacity="0.35"/>
  <text x="508" y="98"  text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">c-&gt;id</text>
  <text x="508" y="116" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">c-&gt;weight (kg)</text>
  <text x="508" y="134" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">c-&gt;dimensions[3]</text>
  <text x="508" y="152" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">c-&gt;type</text>
  <text x="508" y="170" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">c-&gt;dg (ptr / NULL)</text>
  <line x1="458" y1="178" x2="558" y2="178" stroke="currentColor" stroke-width="0.8" opacity="0.35"/>
  <text x="508" y="196" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">pos_x = -1.0f</text>
  <text x="508" y="212" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">pos_y = -1.0f</text>
  <text x="508" y="228" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">pos_z = -1.0f</text>
  <text x="508" y="244" text-anchor="middle" font-size="8.5" fill="#12A594" opacity="0.8">cargo_count++</text>

  <!-- Arrows: tokens → Cargo -->
  <line x1="394" y1="65" x2="450" y2="91" stroke="currentColor" stroke-width="1.1" opacity="0.4" marker-end="url(#arr)"/>
  <line x1="414" y1="106" x2="450" y2="114" stroke="currentColor" stroke-width="1.1" opacity="0.4" marker-end="url(#arr)"/>
  <line x1="426" y1="141" x2="450" y2="133" stroke="#12A594" stroke-width="1.2" opacity="0.7" marker-end="url(#arr-teal)"/>
  <line x1="394" y1="175" x2="450" y2="150" stroke="currentColor" stroke-width="1.1" opacity="0.4" marker-end="url(#arr)"/>
  <line x1="426" y1="209" x2="450" y2="168" stroke="currentColor" stroke-width="1.1" opacity="0.4" marker-end="url(#arr)"/>

  <!-- Error path -->
  <rect x="184" y="248" width="108" height="36" rx="4"
        fill="none" stroke="#D05663" stroke-width="1.4"/>
  <text x="238" y="263" text-anchor="middle" font-size="9.5" fill="#D05663">isnan() → error</text>
  <text x="238" y="276" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.6">free dg, null cargo ptr</text>
  <text x="238" y="287" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.6">return -1</text>
  <line x1="318" y1="106" x2="268" y2="248" stroke="#D05663" stroke-width="1.1" stroke-dasharray="4 3" opacity="0.7" marker-end="url(#arr-red)"/>
  <text x="278" y="235" text-anchor="middle" font-size="8" fill="#D05663" opacity="0.8">NAN</text>

  <!-- memset label -->
  <text x="82" y="188" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.55">memset(c, 0, …) first</text>

  <!-- Arrow markers -->
  <defs>
    <marker id="arr" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="currentColor" opacity="0.55"/>
    </marker>
    <marker id="arr-teal" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="#12A594" opacity="0.7"/>
    </marker>
    <marker id="arr-red" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="#D05663" opacity="0.7"/>
    </marker>
  </defs>
</svg>

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
(from [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)):

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
