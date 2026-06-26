# Validation and robustness

Every program eventually meets bad input — a typo in a weight field, a manifest with a missing dimension, a DG code no one has seen before. CargoForge-C must survive all of that without crashing. This lesson traces the validation strategy embedded in `src/parser.c`, from the single function that guards every numeric field, through the error paths that clean up borrowed memory before returning, to the contract the project holds with its own fuzzer.

## The contract: never crash on bad input

A crash on malformed input is not an acceptable outcome for any safety-relevant program. The CargoForge-C project expresses this as a testable invariant enforced by the fuzzer in `scripts/fuzz.sh`: the binary must never exit with a signal (code ≥ 128). It may reject bad data, print an error, and return a non-zero exit code — that is a clean rejection, not a failure. A crash is always a bug.

Implementing that contract requires two things working together:

1. **Input validation** — detecting the problem before bad data reaches any computation.
2. **Clean error paths** — releasing every resource that was allocated before the bad line arrived, so the caller never sees a partial or dangling state.

Both are visible in `parse_ship_config` and `parse_cargo_list` in `src/parser.c`.

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

`scripts/fuzz.sh` is the live proof that this contract holds. It generates manifests with adversarial values — negative weights, zero, `"abc"`, empty strings, malformed DG codes — and runs the binary under AddressSanitizer. Any exit code ≥ 128 (signal) or any ASan/UBSan message is a FAIL. The tests run with `ASAN_OPTIONS=abort_on_error=1` so that even a single invalid memory access is fatal to the process.

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
