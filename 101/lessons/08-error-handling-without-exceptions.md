# Error handling without exceptions

C gives you no `try`/`catch`, no stack unwinding, no runtime that catches a bad
value and routes it somewhere safe. Every error must be detected, reported, and
propagated by hand. CargoForge-C handles this through four interlocking
mechanisms: integer return codes, `errno`, `NaN` as a floating-point sentinel,
and process exit codes. Understanding them together explains how the program can
reject a malformed manifest without crashing — and why getting it wrong caused a
real heap-use-after-free bug.

## The mental model 🧠

With no `try`/`catch`, an error in C can't teleport up to a handler — it has to be *carried up the stairs by hand*, one function at a time. Each function returns a verdict: `0` for success, `-1` for failure, or `NAN` for a physics result that simply doesn't exist (an unstable ship has no real GM). The caller's job is to *check that verdict and pass it on*. Skip the check, and the bad value keeps flowing downstream as if nothing went wrong.

That is not a hypothetical. A skipped error path is exactly how a parse failure once slid through and became a real heap-use-after-free: the function that should have stopped, cleaned up, and returned `-1` instead kept running on memory it had already freed. The fix wasn't cleverer code — it was making the error propagate honestly and clearing the dangling pointer (`docs/BUG_JOURNAL.md`).

Hold this: **in C, errors don't propagate themselves — you do.** Every `return -1` you ignore is a bug the compiler will never warn you about.

<svg viewBox="0 0 600 240" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>In C, an error is carried up the call chain by hand, one return at a time</title>
<desc>parse_cargo_list hits a bad weight, frees its buffer, and returns -1. parse_ship_config checks that return code and propagates -1 upward; main checks it and exits non-zero. If any caller skips the check, the bad value keeps flowing — the cause of a real use-after-free.</desc>
<rect x="40" y="28" width="360" height="50" rx="6" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.45"/>
<text x="54" y="48" font-size="11.5" fill="currentColor" font-family="var(--md-code-font,monospace)">main()</text>
<text x="54" y="68" font-size="10" fill="currentColor" opacity="0.7" font-family="var(--md-code-font,monospace)">if (rc != 0) return EXIT_FAILURE;</text>
<rect x="40" y="100" width="360" height="50" rx="6" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.45"/>
<text x="54" y="120" font-size="11.5" fill="currentColor" font-family="var(--md-code-font,monospace)">parse_ship_config()</text>
<text x="54" y="140" font-size="10" fill="currentColor" opacity="0.7" font-family="var(--md-code-font,monospace)">if (rc != 0) return -1;</text>
<rect x="40" y="172" width="360" height="50" rx="6" fill="#D05663" fill-opacity="0.07" stroke="#D05663" stroke-opacity="0.6"/>
<text x="54" y="192" font-size="11.5" fill="currentColor" font-family="var(--md-code-font,monospace)">parse_cargo_list()</text>
<text x="54" y="210" font-size="10" fill="#D05663" opacity="0.9" font-family="var(--md-code-font,monospace)">bad weight  →  free + return -1</text>
<line x1="430" y1="186" x2="430" y2="141" stroke="#D05663" stroke-width="1.3"/><path d="M426,148 L430,140 L434,148" fill="none" stroke="#D05663" stroke-width="1.3"/>
<line x1="430" y1="114" x2="430" y2="69" stroke="#D05663" stroke-width="1.3"/><path d="M426,76 L430,68 L434,76" fill="none" stroke="#D05663" stroke-width="1.3"/>
<text x="446" y="167" font-size="10.5" fill="#D05663" font-family="var(--md-code-font,monospace)">-1</text>
<text x="446" y="95" font-size="10.5" fill="#D05663" font-family="var(--md-code-font,monospace)">-1</text>
<text x="515" y="120" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.55">the error is</text>
<text x="515" y="134" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.55">carried up by hand</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Return code** = "a number the function hands back to say whether it worked" — `parse_ship_config` and `parse_cargo_list` return `0` for success and `-1` for failure; `cmd_optimize` checks that number immediately and stops early if something went wrong, instead of blundering forward on bad data.
- **`errno`** = "a global sticky note the C library leaves when it hits an OS-level problem" — `safe_atof` resets it to zero before calling `strtof`, then reads it afterwards; without the reset, a stale value from an earlier call could falsely report an error.
- **`NAN` (Not a Number)** = "a float value that means 'this field is broken'" — `safe_atof` returns it instead of `0` or `-1` because both of those are physically valid numbers for weight or length, while `NAN` is unambiguous and can be tested with `isnan()`.
- **NULL-and-zero after `free()`** = "telling the rest of the code the memory is gone" — after `parse_cargo_list` frees `ship->cargo` on a bad weight, it sets `ship->cargo = NULL` and `ship->cargo_count = 0` in the same block; this is exactly what prevented `ship_cleanup` from walking a freed pointer (the heap-use-after-free bug this code fixed).
- **Exit code** = "the number your program reports to the shell when it finishes" — `EXIT_PARSE_ERROR` and `EXIT_VALIDATION_ERROR` are distinct so that a CI script or shell wrapper can tell apart "the file was unreadable" from "the file parsed but made no physical sense."
- **Warning vs. error** = "recoverable vs. unrecoverable" — a missing hydrostatic table gets a `fprintf(stderr, "Warning: …")` and the program continues with the box-hull fallback; an invalid weight or a file that won't open returns `-1` immediately, because there is no sensible fallback for "the ship has no length."

**Why it matters:** if any one of these mechanisms is skipped — the return value goes unchecked, `errno` is not reset, the freed pointer is not nulled — the program either silently produces wrong stability numbers or crashes with undefined behaviour; the heap-use-after-free bug in the lesson is a direct example of what happens when the NULL-and-zero step is missing.

---

## Return codes: the primary signalling channel

The most common convention in C is for functions that can fail to return an
`int`: **0 on success, -1 (or another negative value) on failure**. The caller
must check the return value — the language will not do it for you.

Both top-level parsers in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) follow this pattern exactly:

```c
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);
```

In [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c), `cmd_optimize` calls them and checks immediately:

```c
if (parse_ship_config(ctx->ship_file, &ship) != 0) {
    print_error_with_context(ctx->ship_file, 0,
                             "Failed to parse ship configuration");
    return EXIT_PARSE_ERROR;
}

if (parse_cargo_list(ctx->cargo_file, &ship) != 0) {
    print_error_with_context(ctx->cargo_file, 0,
                             "Failed to parse cargo manifest");
    ship_cleanup(&ship);
    return EXIT_PARSE_ERROR;
}
```

Notice two things:

1. **Early return.** The moment either parser fails, the function returns. There
   is no "continue and see what happens" — that approach leads to downstream
   functions operating on partially initialised data.
2. **Cleanup before returning.** If the ship config loaded successfully but the
   cargo manifest failed, memory has already been allocated (for `ship.cargo`,
   `ship.hydro`, etc.). The code calls `ship_cleanup(&ship)` before returning so
   that memory is freed. Skipping this step would be a memory leak.

### Exit codes communicate failure to the shell

`EXIT_PARSE_ERROR` is not just a label for readability — it is the value passed
to the operating system when `main` returns or `exit()` is called. The shell
(or any script wrapping CargoForge-C) can test `$?` and branch on it. The
codebase defines named constants so that different failure modes produce distinct
codes that automation can distinguish:

| Constant | Meaning |
|---|---|
| `EXIT_SUCCESS` (0) | Everything worked |
| `EXIT_PARSE_ERROR` | Ship config or cargo manifest was invalid |
| `EXIT_VALIDATION_ERROR` | Manifest was parseable but failed validation |
| `EXIT_INVALID_ARGS` | Wrong number or type of command-line arguments |

`cmd_validate` in [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c) illustrates this clearly: it accumulates an
`errors` counter across both parse calls, then returns `EXIT_SUCCESS` only when
the counter is zero, otherwise `EXIT_VALIDATION_ERROR`.

---

## `errno`: the OS layer of error reporting

When a C standard-library function fails, it sets the global integer `errno` to
a code that describes what went wrong. Common values include `ENOENT` (no such
file), `EACCES` (permission denied), and `ERANGE` (result out of range).

[`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) uses `errno` in two places.

**`perror`** is the quick way to print an `errno`-based message. When
`parse_ship_config` cannot open the config file, it does:

```c
file = fopen(filename, "r");
if (!file) {
    perror("Error opening ship config file");
    return -1;
}
```

`perror` appends the human-readable string for the current `errno` value —
something like `Error opening ship config file: No such file or directory`.
The caller still gets `-1` and handles it; `perror` just ensures the user sees
a meaningful message on `stderr`.

**`strtof` + `errno`** appears inside `safe_atof`, the numeric conversion
helper. The standard `atof` function has no error detection at all: it returns
`0.0` on failure, which is indistinguishable from a legitimate zero value.
`strtof` is safer:

```c
static float safe_atof(const char *s, float min, float max,
                        const char *field_name) {
    char *end = NULL;
    errno = 0;                        // reset before the call
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

The pattern here is worth reading carefully:

- `errno = 0` — reset before the call, because `errno` is never automatically
  cleared. A stale non-zero value from an earlier call would produce a false
  failure.
- `end == s` — `strtof` sets `*end` to the first character it could not
  convert. If `end == s`, the function consumed nothing: the input was not a
  number at all.
- `*end != '\0' && *end != '\n'` — if conversion stopped before the end of the
  string, there was trailing garbage (e.g. `"12abc"`).
- `val < min || val > max` — domain validation: a ship length of `-50` or
  `999999999` is syntactically a number but physically nonsense.
- `errno != 0` — catches overflow/underflow reported by `strtof` itself (e.g.
  `ERANGE` for values beyond `FLT_MAX`).

Any of these conditions causes `safe_atof` to print an error and return `NAN`.

---

## NaN as a floating-point sentinel

`NAN` — "Not a Number" — is a special IEEE 754 floating-point value. It
propagates through arithmetic (any arithmetic on `NAN` yields `NAN`) and can be
tested with `isnan()`. This makes it an effective sentinel for "this value is
not valid."

`safe_atof` returns `NAN` on failure. The caller checks with `isnan`:

```c
float v = safe_atof(value, 0.1f, 1e9f, key);
if (isnan(v)) {
    if (!use_stdin) fclose(file);
    return -1;      // abort parsing on invalid data
}
```

This is cleaner than returning `0` or `-1` from a function that returns a
`float`, because those are legitimate physical values (a zero-weight cargo item
would be suspicious but syntactically valid). `NAN` is unambiguous.

The same sentinel appears in `parse_cargo_list` for the weight field:

```c
float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
if (isnan(weight_t)) {
    for (int j = 0; j < ship->cargo_count; j++)
        free(ship->cargo[j].dg);
    free(ship->cargo);
    ship->cargo = NULL;    // avoid dangling pointer -> use-after-free in ship_cleanup
    ship->cargo_count = 0;
    /* ... close file / free line buffer ... */
    return -1;
}
```

The two lines `ship->cargo = NULL` and `ship->cargo_count = 0` are not
cosmetic. They are the fix for a real heap-use-after-free bug.

!!! warning "The bug this code fixed"
    Before this fix, the error path freed `ship->cargo` but left
    `ship->cargo_count` at its current (non-zero) value and the pointer
    dangling. When `ship_cleanup` was called later, it iterated
    `for (int i = 0; i < ship->cargo_count; i++)` and accessed
    `ship->cargo[i].dg` through the freed pointer — undefined behaviour that
    AddressSanitizer reported as a `heap-use-after-free`. The rule: **NULL the
    pointer and zero the count in the same block as `free()`**.

---

## How the four mechanisms connect

Here is the flow for a single bad weight value in a cargo manifest, traced end
to end:

```
cargo file: "Box1  -5  10x2x2  standard"
                ^^
                negative weight — below min of 0.1

safe_atof("-5", 0.1, 1e6, "weight")
  → errno=0, strtof succeeds (val=-5.0), but val < min
  → prints "Error: Invalid or out-of-range weight value '-5'"
  → returns NAN

parse_cargo_list:
  isnan(weight_t) == true
  → frees already-parsed cargo, NULLs pointer, zeroes count
  → returns -1

cmd_optimize (cli.c):
  parse_cargo_list(...) != 0
  → calls ship_cleanup (safe: cargo pointer is NULL)
  → returns EXIT_PARSE_ERROR

main (main.c):
  dispatch_subcommand returns EXIT_PARSE_ERROR
  → process exits with that code
  → shell sees $? != 0
```

Each layer adds one piece: `strtof`+`errno` detects the problem at the bit
level; `safe_atof` converts it to `NAN` with a human message; `parse_cargo_list`
converts `NAN` to `-1` with cleanup; the CLI converts `-1` to a named exit code
that the shell can act on.

---

## Warnings vs. errors: degraded-mode operation

Not every problem stops the parse. In `parse_ship_config`, if the optional
hydrostatic table fails to load, the code prints a warning and continues with
the box-hull fallback:

```c
if (parse_hydro_table(hydro_path, (HydroTable *)ship->hydro) != 0) {
    free(ship->hydro);
    ship->hydro = NULL;
    fprintf(stderr,
            "Warning: Failed to load hydrostatic table, "
            "using box-hull fallback\n");
}
```

The distinction is intentional. A missing hydrostatic table is recoverable —
the program can still produce a valid (if less accurate) analysis. An invalid
ship length or a weight value of `abc` is not recoverable — there is no
sensible fallback for "the ship has no length." Hard errors abort and return
`-1`; soft errors warn on `stderr` and proceed.

[`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c) uses `print_warning` for messages like "Total cargo weight exceeds
ship capacity!" inside `cmd_validate --verbose`. This reaches `stderr` (not
`stdout`) so that it does not pollute structured output (JSON, CSV) while still
being visible to a human watching the terminal.

---

## Recap

- C returns errors as `int` values (0 = success, -1 = failure); every caller
  must check the return value explicitly.
- `strtof` + `errno` gives robust string-to-float conversion; `safe_atof`
  wraps this with range validation and returns `NAN` on any failure.
- `NAN` propagates cleanly and is tested with `isnan()`; it is unambiguous in
  a way that `0` or `-1` from a float-returning function is not.
- After `free()`, always NULL the pointer and zero the count in the same block;
  this is the only protection against use-after-free in later cleanup code.
- Distinct exit codes (`EXIT_PARSE_ERROR`, `EXIT_VALIDATION_ERROR`, etc.) let
  shell scripts and CI pipelines branch on the precise failure mode.
- Warnings go to `stderr` so structured output on `stdout` stays parseable.

## Check yourself

??? question "C has no try/catch — what three mechanisms does CargoForge-C actually use to signal an error?"
    Integer return codes (0 success, -1 failure), `errno` for system-call-level failures, and `NaN` as a floating-point sentinel for a physics result that doesn't exist. Every caller must actively check the result and decide whether to propagate it.

??? question "What was the actual gap that let the real heap-use-after-free bug through?"
    An error path in `parse_cargo_list` freed `ship->cargo` but didn't finish the job of leaving the ship in a safe state — the pointer stayed non-NULL and the count stayed non-zero — so a later, unrelated call to `ship_cleanup` walked memory that had already been reclaimed.

*Next: [Lab 2 — Read the Data Model](lab-02-c-in-depth.md).*
