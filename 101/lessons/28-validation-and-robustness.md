# Validation and robustness

Every program eventually meets bad input — a typo in a weight field, a manifest with a missing dimension, a DG code no one has seen before. CargoForge-C must survive all of that without crashing. This lesson traces the validation strategy embedded in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c), from the single function that guards every numeric field, through the error paths that clean up borrowed memory before returning, to the contract the project holds with its own fuzzer.

## The mental model 🧠

Treat every input as guilty until proven valid. A manifest is untrusted text — a typo'd weight, a missing dimension, a DG code from the future — and the parser's whole job on the front line is to *reject cleanly*: say no, free anything it borrowed, return −1, and never crash. `safe_atof` is the single bouncer every number must pass; it wraps `strtof` with four checks and returns `NAN` — a value that fails its own equality test, so callers detect it with `isnan` and bail.

Robustness is just as much about what happens *after* you say no. The error path must leave memory in a safe state — the lesson the fuzzer taught the hard way: freeing `ship->cargo` but leaving the pointer non-NULL let `ship_cleanup` walk a dead array (a use-after-free, exit 134). The fix — `free`, then null the pointer and zero the count — plus a two-pass allocation (count the lines, `malloc` once) means every error path has exactly one clean thing to undo. Good rejection is as engineered as good acceptance.

<svg viewBox="0 0 600 200" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>safe_atof is the bouncer every number must pass</title>
<desc>safe_atof wraps strtof with four checks — overflow, at least one digit, no trailing garbage, and in range. A token that passes all four becomes a trusted float; failing any check returns NAN, which the caller detects with isnan and rejects cleanly with -1.</desc>
<rect x="14" y="44" width="64" height="34" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="46" y="65" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">"1200"</text>
<g font-size="9.5" text-anchor="middle">
<rect x="100" y="44" width="92" height="34" rx="5" fill="#12A594" fill-opacity="0.08" stroke="#12A594" stroke-opacity="0.6"/><text x="146" y="65" fill="currentColor">overflow?</text>
<rect x="208" y="44" width="92" height="34" rx="5" fill="#12A594" fill-opacity="0.08" stroke="#12A594" stroke-opacity="0.6"/><text x="254" y="65" fill="currentColor">has digits?</text>
<rect x="316" y="44" width="92" height="34" rx="5" fill="#12A594" fill-opacity="0.08" stroke="#12A594" stroke-opacity="0.6"/><text x="362" y="65" fill="currentColor">no junk?</text>
<rect x="424" y="44" width="92" height="34" rx="5" fill="#12A594" fill-opacity="0.08" stroke="#12A594" stroke-opacity="0.6"/><text x="470" y="65" fill="currentColor">in range?</text>
</g>
<g stroke="#12A594" stroke-opacity="0.6">
<line x1="78" y1="61" x2="98" y2="61"/><line x1="192" y1="61" x2="206" y2="61"/><line x1="300" y1="61" x2="314" y2="61"/><line x1="408" y1="61" x2="422" y2="61"/>
</g>
<line x1="516" y1="61" x2="536" y2="61" stroke="#12A594" stroke-opacity="0.7"/><path d="M529,57 L536,61 L529,65" fill="none" stroke="#12A594"/>
<text x="560" y="58" font-size="10" text-anchor="middle" fill="#12A594">1200.0</text>
<text x="560" y="72" font-size="8" text-anchor="middle" fill="currentColor" opacity="0.6">trusted</text>
<g stroke="#D05663" stroke-opacity="0.55" stroke-dasharray="4 3">
<line x1="146" y1="78" x2="146" y2="128"/><line x1="254" y1="78" x2="254" y2="128"/><line x1="362" y1="78" x2="362" y2="128"/><line x1="470" y1="78" x2="470" y2="128"/>
</g>
<line x1="146" y1="128" x2="470" y2="128" stroke="#D05663" stroke-opacity="0.5"/>
<rect x="220" y="136" width="180" height="40" rx="5" fill="#D05663" fill-opacity="0.08" stroke="#D05663" stroke-opacity="0.6"/>
<text x="310" y="154" font-size="10" text-anchor="middle" fill="currentColor">any check fails → NAN</text>
<text x="310" y="169" font-size="9" text-anchor="middle" fill="#D05663" opacity="0.9" font-family="var(--md-code-font,monospace)">isnan(v) → return -1</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Clean rejection** = "the program says no and stops safely, instead of crashing" — `parse_ship_config` and `parse_cargo_list` both return −1 on bad input and leave the caller's memory in a safe state, which is what "clean rejection" looks like in practice.
- **`safe_atof`** = "the one bouncer that checks every number before it's let in" — it wraps `strtof` with four checks (overflow, no digits, trailing garbage, out-of-range) and returns `NAN` as an unambiguous "refused entry" signal so the caller can test `isnan(v)` and bail immediately.
- **NAN sentinel** = "a special float value that means 'this field failed'" — because `NAN != NAN` is always true in IEEE 754, the only reliable way to detect it is `isnan(val)`, which is exactly what every caller of `safe_atof` does.
- **Null-after-free** = "zeroing a pointer the moment you free it, so no one accidentally uses it again" — after freeing `ship->cargo` in the error path, the code immediately sets `ship->cargo = NULL` and `ship->cargo_count = 0`; this is what stopped `ship_cleanup` from walking a freed array and triggering a heap-use-after-free.
- **Use-after-free** = "reading memory you already gave back, which can silently corrupt data or crash" — the fuzzer caught exactly this bug in `parse_cargo_list`: freeing the cargo array but leaving the pointer non-NULL meant `ship_cleanup` later iterated it with a stale count, and AddressSanitizer flagged the access (exit code 134).
- **Two-pass allocation** = "count the lines first, allocate once, then parse" — `parse_cargo_list` scans the file once to count non-comment lines and calls `malloc` a single time, so every subsequent error path only has one array to free rather than a chain of partial allocations.
- **Fuzzer as enforcer** = "a script that throws adversarial inputs at the binary and treats any crash (exit ≥ 128) as a test failure" — `scripts/fuzz.sh` running under AddressSanitizer is what turned the use-after-free from a silent bug into a loud, reproducible FAIL.

**Why it matters:** a ship-stability program that crashes on a bad manifest is not just annoying — it could leave a partially-loaded cargo plan in memory that a downstream analysis trusts. Getting validation and cleanup right means the only outcome for bad input is a clear error message, never a corrupted or partially-freed state.

---

## The contract: never crash on bad input

A crash on malformed input is not an acceptable outcome for any safety-relevant program. The CargoForge-C project expresses this as a testable invariant enforced by the fuzzer in [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh): the binary must never exit with a signal (code ≥ 128). It may reject bad data, print an error, and return a non-zero exit code — that is a clean rejection, not a failure. A crash is always a bug.

Implementing that contract requires two things working together:

1. **Input validation** — detecting the problem before bad data reaches any computation.
2. **Clean error paths** — releasing every resource that was allocated before the bad line arrived, so the caller never sees a partial or dangling state.

Both are visible in `parse_ship_config` and `parse_cargo_list` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c).

## `safe_atof` — the numeric gatekeeper

All numeric values in CargoForge-C pass through one function before touching any struct field:

```c
// src/parser.c
static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    errno = 0;
    float val = strtof(s, &end);

    if (errno != 0 || end == s || (*end != '\0' && *end != '\n') || val < min || val > max) {
        fprintf(stderr, "Error: Invalid or out-of-range %s value '%s'\n", field_name, s);
        return NAN;
    }
    return val;
}
```

Four checks happen in a single condition:

| Check | Detects |
|---|---|
| `errno != 0` | Overflow or underflow reported by `strtof` |
| `end == s` | No digits were parsed at all (e.g. `"abc"`, `""`) |
| `*end != '\0' && *end != '\n'` | Trailing garbage after the number (e.g. `"12.4xyz"`) |
| `val < min \|\| val > max` | Value outside the caller-supplied range |

`strtof` (not `atof`) is used deliberately. `atof` returns 0 on failure with no way to distinguish success; `strtof` sets `end` to the first character it could not convert, so even `"0"` can be distinguished from `"not-a-number"` by checking whether `end` advanced.

The caller-supplied `min` and `max` make the range semantic — ship config fields use `[0.1, 1e9]`, weight fields use `[0.1, 1e6]`, dimensions use `[0.1, 1e4]`. The field name appears in the error message so the user can fix the right line.

The return value `NAN` (Not a Number from `<math.h>`) is a sentinel that propagates without ambiguity: the caller tests it with `isnan(v)` immediately after the call.

!!! note
    `NAN` is the right sentinel here because `float` has no reserved "error" bit pattern visible to plain comparisons. `val == NAN` is always false in IEEE 754 — only `isnan(val)` reliably detects it.

## Rejecting bad ship config

In `parse_ship_config`, every numeric key goes through `safe_atof`. If the call returns `NAN`, the function closes the file (if it was opened) and returns `−1` immediately:

```c
// src/parser.c
float v = safe_atof(value, 0.1f, 1e9f, key);
if (isnan(v)) {
    if (!use_stdin) fclose(file);
    return -1;
}
```

At this point in the function, the only heap memory that might be open is the file handle itself. The `Ship` struct is stack-allocated by the caller, and the `hydro`, `tanks`, and `strength_limits` sub-objects are allocated only *after* the key=value loop completes. So closing the file and returning is sufficient — there is nothing on the heap to leak.

String-valued keys (`hydrostatic_table`, `tank_config`) skip `safe_atof` entirely and are stored in local arrays. If the corresponding files fail to load later, the code degrades gracefully:

```c
// src/parser.c
if (parse_hydro_table(hydro_path, (HydroTable *)ship->hydro) != 0) {
    free(ship->hydro);
    ship->hydro = NULL;
    fprintf(stderr, "Warning: Failed to load hydrostatic table, using box-hull fallback\n");
}
```

This is a deliberate design decision: a missing hydrostatic table is survivable (the analysis falls back to the box-hull model), so it produces a warning, not a fatal error. The distinction between *errors that must abort* and *warnings that degrade gracefully* is an explicit part of the contract.

## Rejecting bad cargo — and cleaning up after yourself

The cargo manifest parser is more involved because it allocates a heap array *before* reading each line. If a line fails validation, memory already allocated for earlier, valid cargo items must be freed before returning.

### Two-pass allocation

For file-based manifests, `parse_cargo_list` does a first pass to count non-comment lines, then allocates the `Ship.cargo` array exactly once:

```c
// src/parser.c
int count = 0;
while (fgets(line, sizeof(line), file)) {
    if (line[0] != '#' && line[0] != '\n') count++;
}
ship->cargo = malloc(count * sizeof(Cargo));
```

After this call, `ship->cargo` is live heap memory. Every error path that follows is responsible for freeing it.

### The error path that taught a lesson

Consider what happens when `safe_atof` rejects a weight value:

```c
// src/parser.c
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;   // avoid a dangling pointer -> use-after-free/double-free in ship_cleanup
    ship->cargo_count = 0;
    if (use_stdin) {
        for (int j = 0; j < line_count; j++) free(lines[j]);
        free(lines);
    } else {
        fclose(file);
    }
    return -1;
}
```

The comment on the `NULL` assignment is not decorative — it records a real bug that the fuzzer caught. The original code freed `ship->cargo` but left `ship->cargo` pointing at the freed memory and `ship->cargo_count` at its non-zero value. When `ship_cleanup` was later called by the CLI, it iterated the cargo array using the stale count and dereferenced the freed pointer to access `cargo[i].dg`. That is a **heap-use-after-free**.

AddressSanitizer, enabled by `make test-asan`, flagged the access and aborted the process (exit code 134, ≥ 128 → fuzzer FAIL). The fix is the two lines that follow the `free`:

```c
ship->cargo = NULL;
ship->cargo_count = 0;
```

`ship_cleanup` guards against NULL before touching the array, so setting the pointer to NULL makes the cleanup a safe no-op for the cargo field. Setting the count to zero ensures no iteration happens even if the guard were removed.

The identical pattern appears in the dimension-parse error path a few lines below:

```c
// src/parser.c
if (!dims_ok) {
    fprintf(stderr, "Error: Incomplete or invalid dimensions for cargo '%s' on line %d\n", id, line_num);
    for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;
    ship->cargo_count = 0;
    /* ... close file / free lines ... */
    return -1;
}
```

### DG field validation

The optional fifth field is handled by `parse_dg_field`. It allocates a `DGInfo` struct on the heap and fills it before applying a validation check at the end:

```c
// src/parser.c (parse_dg_field)
if (dg->dg_class < 1 || dg->dg_class > 9) {
    free(dg);
    return NULL;
}
return dg;
```

This is a clean allocation-validate-or-free pattern: allocate, fill, check, and free if invalid. The caller receives either a valid pointer or NULL. Because `parse_dg_field` returns NULL for non-DG fields (those not starting with `DG:`), the main parse loop does not need special cases — a NULL result simply means "no DG info":

```c
// src/parser.c
if (dg_field) {
    c->dg = (struct DGInfo_ *)parse_dg_field(dg_field);
}
```

## Putting it together: the validation flow

```
parse_cargo_list
│
├── allocate ship->cargo                     (may fail → return -1)
│
└── for each cargo line:
    ├── safe_atof(weight)     → NAN?
    │   └── free DG of committed items
    │       free ship->cargo; ship->cargo = NULL; ship->cargo_count = 0
    │       close file; return -1
    │
    ├── safe_atof(dim × 3)    → NAN?
    │   └── same cleanup as above
    │
    └── parse_dg_field()      → NULL = non-DG or invalid class → safe, no-op
```

Every exit point leaves the `Ship` in a state where a subsequent call to `ship_cleanup` does the right thing: frees what is present, ignores what is NULL.

## The fuzzer as enforcer

[`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) is the live proof that this contract holds. It generates manifests with adversarial values — negative weights, zero, `"abc"`, empty strings, malformed DG codes — and runs the binary under AddressSanitizer. Any exit code ≥ 128 (signal) or any ASan/UBSan message is a FAIL. The tests run with `ASAN_OPTIONS=abort_on_error=1` so that even a single invalid memory access is fatal to the process.

The heap-use-after-free in the original `parse_cargo_list` was found exactly this way: a manifest with `"-5"` as the weight field triggered the error path, and ASan caught the subsequent access through the dangling pointer in `ship_cleanup`.

!!! warning
    AddressSanitizer is not free. Build with `make test-asan` to catch memory errors during development; ship with `make` (optimised, no sanitizers) for performance. Never disable sanitizers to silence a failure — find the root cause instead.

## Recap

- `safe_atof` is the single validation point for all numeric input: it combines `strtof`'s robustness, a caller-supplied range check, and a `NAN` sentinel to signal failure without ambiguity.
- Ship config and cargo parse functions return `−1` on any validation failure; the caller is responsible for checking the return value.
- Every error path in `parse_cargo_list` frees already-allocated memory *and* nulls the pointer and zeroes the count before returning, so that `ship_cleanup` can run safely regardless of when or why parsing failed.
- Setting a freed pointer to NULL immediately after `free` is not a style choice — it is the only reliable way to prevent a use-after-free when the pointer will be read again by any other code path.
- The fuzzer encodes the core contract as a machine-checked test: clean rejection of bad input is required; crashing is a bug.

*Next: [Dangerous-goods (IMDG) parsing](29-dangerous-goods-imdg-parsing.md).*
