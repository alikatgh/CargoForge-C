# Memory bugs (the real ones)

Memory errors are the most dangerous class of bugs in C: they are silent, they corrupt program state invisibly, and they are the primary vector for security vulnerabilities. This lesson uses a real heap-use-after-free that CargoForge-C's own fuzzer caught in `parse_cargo_list` to teach the four canonical memory bug types — and the small, disciplined habits that prevent all of them.

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

CargoForge-C uses `strncpy` with `sizeof(buf) - 1` everywhere exactly to prevent this. In `parse_dg_field` in `src/parser.c`:

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

### The setup

`parse_cargo_list` (in `src/parser.c`) reads a whitespace-delimited manifest of cargo items into a heap-allocated array on `ship->cargo`. The array is allocated before the parsing loop, because the function needs to know how many items there are before it can fill them.

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

`scripts/fuzz.sh` builds a sanitised binary with AddressSanitizer and UBSan enabled, then fires randomly generated malformed inputs at the `optimize` and `validate` subcommands:

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

The fix is two lines, applied in both error paths in `src/parser.c`:

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
- **AddressSanitizer + fuzzing** is what actually found it: `make test-asan` and `scripts/fuzz.sh` together force error paths that normal tests never exercise.

*Next: [Lab 3 — Reproduce the Use-After-Free](lab-03-memory.md).*
