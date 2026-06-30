# Memory bugs (the real ones)

Memory errors are the most dangerous class of bugs in C: they are silent, they corrupt program state invisibly, and they are the primary vector for security vulnerabilities. This lesson uses a real heap-use-after-free that CargoForge-C's own fuzzer caught in `parse_cargo_list` to teach the four canonical memory bug types — and the small, disciplined habits that prevent all of them.

## The mental model 🧠

All four classic C memory bugs are the same mistake wearing different clothes: a gap between what you *think* you own and what you *actually* own. **Use-after-free** is reading a library book after you returned it. A **dangling pointer** is keeping the old key after the lock was changed — it still looks valid, which is exactly what makes the bug invisible. A **double free** is trying to return the same book twice. A **leak** is never returning it at all.

CargoForge's own fuzzer caught a real one: an error path in `parse_cargo_list` called `free(ship->cargo)` but left the pointer holding the dead address, so `ship_cleanup` later read memory the allocator had already reclaimed. The fix is the single habit that kills the whole family — *free, then immediately set the pointer to `NULL`* — so a stale read hits an honest crash instead of silently corrupting state. AddressSanitizer (Lesson 38) is the tool that turns these silent bugs loud.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Use-after-free** = "you returned a library book, then kept reading from it anyway" — after `free(ship->cargo)` in `parse_cargo_list`'s error path, the pointer still held the old address, so `ship_cleanup` read straight into memory that the allocator had already reclaimed.
- **Dangling pointer** = "a door key that still looks like a key, but the lock was changed" — `ship->cargo` still contained a valid-looking address after `free()`, which is exactly what made the bug invisible until AddressSanitizer caught it.
- **Double-free** = "trying to return the same library book twice and crashing the librarian's system" — because `parse_cargo_list` freed `ship->cargo` on the error path but didn't null it, `ship_cleanup` then freed the same block a second time, corrupting allocator bookkeeping.
- **`ship->cargo = NULL` after `free`** = "tearing up the key so nobody can use it" — setting the pointer to NULL immediately after freeing means any later accidental dereference produces a clean, catchable segfault instead of silent data corruption; it also makes `free(NULL)` a defined no-op, preventing double-free for free.
- **`ship->cargo_count = 0` alongside the NULL** = "closing both doors, not just one" — `ship_cleanup` loops up to `cargo_count`; zeroing the count ensures the loop never starts even if the NULL check were somehow missed, removing both preconditions for the bug.
- **AddressSanitizer + fuzzer** = "a trip-wire that fires the moment bad memory is touched, combined with a bot that feeds the program deliberately broken manifests" — `make test-asan` and `scripts/fuzz.sh` together forced the error path that normal happy-path tests never reached, which is the only reason this bug was found before it shipped.

**Why it matters:** a use-after-free on an error path is silent in normal testing and only surfaces when an adversarial or malformed input triggers the cleanup sequence — exactly the scenario a ship's cargo system will encounter in production. Get the `free` / `NULL` / count discipline wrong and you hand an attacker a heap-corruption primitive.

---

## The four categories

Before walking through the real bug, you need a clear mental model of what can go wrong. C gives you raw memory; it gives you no guardrails.

### Use-after-free

You `free()` a block of memory and then read or write through a pointer that still points to it. The block may have been re-used by another `malloc` call in the meantime, so you are reading someone else's data — or overwriting it. The program may crash, silently corrupt results, or do nothing observable until much later. All three outcomes are worse than an immediate error.

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
*p = 99;   // use-after-free: the block no longer belongs to you
```

### Double-free

You call `free()` on the same pointer twice. The allocator's internal bookkeeping is now corrupt. This is undefined behaviour; on most systems it eventually causes a crash or opens an exploitable heap-corruption path.

```c
char *buf = malloc(64);
free(buf);
free(buf);   // double-free: undefined behaviour
```

The usual trigger: two different code paths both believe they own the same pointer and both clean up.

### Buffer overflow

You write (or read) past the end of an allocated buffer. In C there is no bounds check. You silently overwrite adjacent memory — possibly another struct's fields, a return address on the stack, or allocator metadata. Stack buffer overflows have historically been among the most exploited vulnerabilities in software.

```c
char id[32];
strncpy(id, user_input, sizeof(id) - 1);   // correct: leaves room for '\0'
strcpy(id, user_input);                     // wrong: overflows if input > 31 bytes
```

CargoForge-C uses `strncpy` with `sizeof(buf) - 1` everywhere exactly to prevent this. In `parse_dg_field` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c):

```c
char buf[64];
strncpy(buf, field + 3, sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';
```

The explicit null termination on the next line closes the `strncpy` edge case: `strncpy` does not null-terminate when the source is exactly `n` bytes long.

### Memory leak

You allocate memory and never free it. The pointer goes out of scope; the memory is no longer reachable but is not returned to the allocator. On long-running processes (or CargoForge-C's `serve` subcommand) leaks accumulate until the OS terminates the process.

Leaks are the least immediately dangerous of the four — they rarely cause incorrect output — but they mask other bugs and indicate the programmer's mental model of ownership is incomplete.

---

## The real bug: heap-use-after-free in `parse_cargo_list`

This is not a textbook example. This is a bug that lived in CargoForge-C until the fuzzer found it.

<svg viewBox="0 0 660 280" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:640px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The use-after-free lifecycle and the one-line fix</title>
<desc>Three snapshots. 1: malloc gives ship->cargo a heap block it owns. 2: on a parse error free() releases the block but ship->cargo still points at it — a dangling pointer. 3: ship_cleanup reads through the dangling pointer and frees it again — use-after-free plus double-free. The fix sets ship->cargo to NULL after freeing, so cleanup's if-check skips it.</desc>
<defs>
<marker id="a-ok" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="currentColor"/></marker>
<marker id="a-bad" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="#D05663"/></marker>
<marker id="a-fix" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="#12A594"/></marker>
<pattern id="hatch" width="6" height="6" patternTransform="rotate(45)" patternUnits="userSpaceOnUse"><line x1="0" y1="0" x2="0" y2="6" stroke="#D05663" stroke-width="1.4" opacity="0.5"/></pattern>
</defs>
<!-- panel 1: allocate -->
<text x="30" y="30" fill="currentColor" font-size="12.5" font-weight="600">1 · malloc</text>
<rect x="30" y="44" width="92" height="26" rx="4" fill="none" stroke="currentColor" stroke-width="1" opacity="0.7"/><text x="76" y="61" fill="currentColor" font-size="11.5" text-anchor="middle">ship-&gt;cargo</text>
<line x1="122" y1="57" x2="158" y2="57" stroke="currentColor" stroke-width="1.4" marker-end="url(#a-ok)"/>
<rect x="160" y="42" width="58" height="30" rx="4" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.2"/><text x="189" y="61" fill="#12A594" font-size="11" text-anchor="middle">Cargo[]</text>
<text x="30" y="92" fill="currentColor" font-size="11" opacity="0.6">owns the array</text>
<!-- panel 2: free, dangling -->
<text x="250" y="30" fill="currentColor" font-size="12.5" font-weight="600">2 · free() on error</text>
<rect x="250" y="44" width="92" height="26" rx="4" fill="none" stroke="currentColor" stroke-width="1" opacity="0.7"/><text x="296" y="61" fill="currentColor" font-size="11.5" text-anchor="middle">ship-&gt;cargo</text>
<line x1="342" y1="57" x2="378" y2="57" stroke="#D05663" stroke-width="1.4" stroke-dasharray="4 3" marker-end="url(#a-bad)"/>
<rect x="380" y="42" width="58" height="30" rx="4" fill="url(#hatch)" stroke="#D05663" stroke-width="1.2" stroke-dasharray="3 2"/><text x="409" y="61" fill="#D05663" font-size="10.5" text-anchor="middle">freed</text>
<text x="250" y="92" fill="#D05663" font-size="11" opacity="0.85">pointer left dangling</text>
<!-- panel 3: cleanup -->
<text x="470" y="30" fill="currentColor" font-size="12.5" font-weight="600">3 · ship_cleanup</text>
<rect x="470" y="44" width="92" height="26" rx="4" fill="none" stroke="currentColor" stroke-width="1" opacity="0.7"/><text x="516" y="61" fill="currentColor" font-size="11.5" text-anchor="middle">ship-&gt;cargo</text>
<line x1="562" y1="57" x2="598" y2="57" stroke="#D05663" stroke-width="1.4" stroke-dasharray="4 3" marker-end="url(#a-bad)"/>
<rect x="600" y="42" width="40" height="30" rx="4" fill="url(#hatch)" stroke="#D05663" stroke-width="1.2"/><text x="620" y="62" fill="#D05663" font-size="15" text-anchor="middle" font-weight="700">✗</text>
<text x="470" y="92" fill="#D05663" font-size="11" opacity="0.85">reads it → UAF + double free</text>
<!-- divider -->
<line x1="30" y1="124" x2="640" y2="124" stroke="currentColor" stroke-width="1" opacity="0.15"/>
<!-- fix lane -->
<text x="30" y="156" fill="#12A594" font-size="12.5" font-weight="700">The one-line fix</text>
<rect x="30" y="174" width="92" height="26" rx="4" fill="none" stroke="currentColor" stroke-width="1" opacity="0.7"/><text x="76" y="191" fill="currentColor" font-size="11.5" text-anchor="middle">ship-&gt;cargo</text>
<line x1="122" y1="187" x2="170" y2="187" stroke="#12A594" stroke-width="1.6" marker-end="url(#a-fix)"/>
<text x="184" y="192" fill="#12A594" font-size="15" font-weight="700">⏚ NULL</text>
<text x="270" y="184" fill="currentColor" font-size="12" opacity="0.8">After freeing, set <tspan fill="#12A594" font-weight="600">ship-&gt;cargo = NULL</tspan>.</text>
<text x="270" y="202" fill="currentColor" font-size="12" opacity="0.8">Now ship_cleanup's <tspan font-family="var(--md-code-font,monospace)">if (ship-&gt;cargo)</tspan> skips it — safe.</text>
<text x="30" y="244" fill="currentColor" font-size="11.5" opacity="0.55">A freed pointer is a loaded gun. The discipline: free, then immediately NULL anything another path can reach.</text>
</svg>

### The setup

`parse_cargo_list` (in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)) reads a whitespace-delimited manifest of cargo items into a heap-allocated array on `ship->cargo`. The array is allocated before the parsing loop, because the function needs to know how many items there are before it can fill them.

```c
ship->cargo = malloc(count * sizeof(Cargo));
ship->cargo_capacity = count;
ship->cargo_count = 0;
```

`cargo_count` starts at zero. Each time a cargo item is successfully parsed, `cargo_count` is incremented.

### The vulnerable error path

Deep inside the parsing loop, if a weight value fails validation — `safe_atof` returns `NAN` — the old code bailed out:

```c
// OLD CODE (before fix) — do not write this
free(ship->cargo);
fclose(file);
return -1;
```

This looks reasonable in isolation. But it left two problems:

1. `ship->cargo` still holds the now-freed address — a **dangling pointer**.
2. `ship->cargo_count` still holds its current value (say, 3), not zero.

The caller (the CLI or libcargoforge) will eventually call `ship_cleanup` to release all ship resources. `ship_cleanup` iterates over the cargo array to free any heap-allocated `DGInfo` structs:

```c
for (int i = 0; i < ship->cargo_count; i++)
    free(ship->cargo[i].dg);   // <-- reads through the dangling pointer
free(ship->cargo);             // <-- double-free
```

`ship->cargo_count` is non-zero, so the loop runs. `ship->cargo` is a dangling pointer to freed memory. The read of `ship->cargo[i].dg` is a **heap-use-after-free**. The subsequent `free(ship->cargo)` is a **double-free**. Both are triggered by the same root cause: the pointer was freed but not nulled.

The same pattern appeared in two separate error paths inside `parse_cargo_list`: one for weight parse failure, one for invalid dimensions.

### How the fuzzer found it

[`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) builds a sanitised binary with AddressSanitizer and UBSan enabled, then fires randomly generated malformed inputs at the `optimize` and `validate` subcommands:

```bash
cc -O1 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
   -fsanitize=address,undefined -fno-omit-frame-pointer src/*.c -lm -o "$SAN"
export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stacktrace=1"
```

The adversarial value corpus (`VALS`) includes entries like `-5`, `0.0`, `abc`, and empty strings:

```bash
VALS=(150 25 -5 0 0.0 abc 1e9 99999999999 3.14 "" "  " /nonexistent.csv)
```

Cargo manifest lines are generated with these values as the weight field:

```bash
printf 'ID%d %s %sx%sx%s %s %s\n' "$RANDOM" "$w" "$w" "$w" "$w" "$t" "$d" >> "$manifest"
```

When `abc` or `-5` lands in the weight column, `safe_atof` returns `NAN`, the error path fires, `ship->cargo` is freed, and then `ship_cleanup` walks the dangling pointer. AddressSanitizer catches the invalid access immediately and aborts with exit code 134. The fuzzer's failure condition checks for exactly this:

```bash
if [ "$rc" -ge 128 ] || grep -qiE 'AddressSanitizer|runtime error:|UndefinedBehavior' "$err"; then
    echo "FUZZ FAIL (iter $i, sub=$sub, exit $rc):" >&2
    ...
    fail=1; break
fi
```

Exit code ≥ 128 means the process was killed by a signal — the fingerprint of a sanitiser abort.

!!! note "The contract"
    The fuzzer's pass/fail criterion is deliberately narrow: "never crash on bad input." A clean non-zero exit code — the parser rejecting malformed data with a printed error — is a **pass**. Only crashes and sanitiser reports are failures. This is the right contract: users will supply bad manifests; the program must handle them without undefined behaviour.

### The fix

The fix is two lines, applied in both error paths in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c):

```c
// Weight parse failure error path (parser.c:333–344)
for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
free(ship->cargo);
ship->cargo = NULL;    // avoid a dangling pointer -> use-after-free/double-free in ship_cleanup
ship->cargo_count = 0;
if (use_stdin) {
    for (int j = 0; j < line_count; j++) free(lines[j]);
    free(lines);
} else {
    fclose(file);
}
return -1;
```

```c
// Dimension parse failure error path (parser.c:362–372)
for (int j = 0; j < ship->cargo_count; j++) free(ship->cargo[j].dg);
free(ship->cargo);
ship->cargo = NULL;    // avoid a dangling pointer -> use-after-free/double-free in ship_cleanup
ship->cargo_count = 0;
if (use_stdin) {
    for (int j = 0; j < line_count; j++) free(lines[j]);
    free(lines);
} else {
    fclose(file);
}
return -1;
```

Two assignments make the dangling pointer safe: `ship->cargo = NULL` prevents any future dereference from reaching freed memory, and `ship->cargo_count = 0` prevents `ship_cleanup`'s loop from even beginning. `ship_cleanup` guards its body:

```c
if (ship->cargo) { ... }
```

A NULL pointer makes the entire cleanup a no-op for the cargo array, which is exactly correct — there is nothing to clean up.

This is the canonical C idiom for safe deallocation:

```c
free(ptr);
ptr = NULL;   // always, immediately, on the next line
```

It costs nothing at runtime and prevents an entire class of bugs.

---

## Why `NULL` after `free` is non-negotiable

The core insight is this: `free(p)` releases the *memory*, but it does not change `p`. After the call, `p` still holds the old address. It is a lie — the address looks valid but the allocator has reclaimed the memory and may hand it to the next `malloc` call.

Nulling the pointer immediately makes the lie visible:

- Any subsequent dereference (`*p`, `p->field`, `p[i]`) will produce a segfault on a null dereference — caught at the moment of misuse, not later when some unrelated allocation happens to land at the same address.
- Double-free is prevented: `free(NULL)` is defined by the C standard as a no-op.

```c
// Safe pattern — always write it this way
free(ptr);
ptr = NULL;
```

The same principle applied to the count variable: setting `cargo_count = 0` removes the other precondition for the loop — even if the pointer were not null, a count of zero means zero iterations, zero dereferences.

---

## Sanitisers: your first line of defence

These bugs are undetectable by reading code carefully. You need tools.

**AddressSanitizer (ASan)** instruments every heap access at compile time. When your program reads or writes freed memory, ASan intercepts the access, prints the exact line number, the allocation site, and the free site, then aborts. It detects use-after-free, double-free, heap buffer overflow, stack buffer overflow, and more.

**UndefinedBehaviorSanitizer (UBSan)** catches signed integer overflow, null pointer dereference, misaligned access, and other C undefined behaviour that ASan doesn't cover.

CargoForge-C builds both into a dedicated binary via `make test-asan`:

```
make test-asan
```

This rebuilds all test binaries with:

```
-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
```

The `-g` flag keeps debug symbols so error reports include file names and line numbers. The `-fno-omit-frame-pointer` flag ensures stack traces are complete.

!!! warning "Sanitisers are not substitutes for correct code"
    ASan only catches a memory error when the bad code path is actually *executed*. A use-after-free on an error path may never be triggered by your happy-path test suite. This is why CargoForge-C pairs `make test-asan` with the fuzzer — the fuzzer forces error paths that unit tests never reach.

---

## Valgrind: the complementary tool

`make test-valgrind` runs each test binary and one integration run through Valgrind with `--leak-check=full`:

```
valgrind --leak-check=full --error-exitcode=1 ./build/test_parser
```

Valgrind is slower (10–50× slowdown) but does not require recompilation. It catches the same memory errors as ASan and additionally reports **memory leaks**: allocations that were never freed by program exit. It is especially useful for catching leaks in rarely-exercised teardown paths.

| Tool | Compile-time cost | Runtime cost | What it catches |
|---|---|---|---|
| ASan | Recompile needed | ~2× slowdown | UAF, double-free, overflow |
| UBSan | Recompile needed | ~1.5× slowdown | UB (overflow, null deref, …) |
| Valgrind | None | 10–50× slowdown | UAF, leaks, uninitialised reads |

---

## The ownership discipline that prevents all four

Every heap allocation has exactly one owner. The owner is responsible for freeing. Writing it down before you write the code prevents most bugs:

| Question | Answer in `parse_cargo_list` |
|---|---|
| Who allocates `ship->cargo`? | `parse_cargo_list` |
| Who owns it after the function returns? | The caller (the `Ship`) |
| Who frees it on the success path? | `ship_cleanup` |
| Who frees it on the error path? | `parse_cargo_list` itself, before returning -1 |
| What must happen immediately after `free`? | `ship->cargo = NULL; ship->cargo_count = 0;` |

When an error path owns the free, it must also zero all state that any future code might use to reach the freed memory. Both the pointer *and* the count.

---

## Recap

- **Use-after-free** reads or writes through a pointer after `free()`. The pointer holds a stale address; the allocator may have re-used that memory.
- **Double-free** calls `free()` twice on the same pointer, corrupting allocator bookkeeping. It is prevented for free by `ptr = NULL` after every free.
- **Buffer overflow** writes past the end of a buffer. Use `strncpy` with `sizeof(buf) - 1` and always write the null terminator explicitly.
- **Memory leak** loses a pointer without freeing it. Valgrind's `--leak-check=full` finds leaks; `ship_cleanup` is CargoForge-C's single teardown point.
- The **real bug** in `parse_cargo_list` was a freed `ship->cargo` pointer left non-null, with a non-zero `cargo_count`, so `ship_cleanup` walked into freed memory. Two lines fixed it: `ship->cargo = NULL; ship->cargo_count = 0;`.
- **AddressSanitizer + fuzzing** is what actually found it: `make test-asan` and [`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh) together force error paths that normal tests never exercise.

*Next: [Lab 3 — Reproduce the Use-After-Free](lab-03-memory.md).*
