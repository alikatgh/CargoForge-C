# Sanitizers (ASan/UBSan)

Unit tests tell you what your program *should* do. Sanitizers tell you what it *is* doing with memory at runtime. AddressSanitizer (ASan) and Undefined Behaviour Sanitizer (UBSan) are compiler-level tools that instrument every memory access and arithmetic operation and report violations the moment they occur — not after the program silently corrupts data downstream. CargoForge-C uses both as a mandatory gate in its CI pipeline, and they caught the real heap-use-after-free bug that lived in `parse_cargo_list`.

## The mental model 🧠

A sanitizer is a tripwire threaded through your program that fires the instant it touches memory it shouldn't. Normally a use-after-free or an out-of-bounds read is *silent* — the program limps on with corrupt data and maybe crashes much later, far from the cause. Compile with `-fsanitize=address` and the compiler keeps a shadow ledger of every byte (live, freed, or never allocated) and checks it on every access; the moment the code reads freed memory it stops *immediately* and prints the exact file, line, and the allocation involved.

That immediacy is the whole value. The `parse_cargo_list` use-after-free was invisible in normal runs, but ASan turned it into a loud abort pointing straight at the bad read — which is why CI runs the suite a second time under it (`make test-asan`, a full clean rebuild, since you cannot mix instrumented and plain object files). **UBSan** is its sibling for *undefined behaviour* — signed overflow, bad shifts, misaligned reads — the silent rule-breaking C otherwise lets slide. Sanitizers cost speed, so they ride in testing, paired with the fuzzer, never in production.

<svg viewBox="0 0 600 232" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>A sanitizer turns a silent memory bug into an immediate, located crash</title>
<desc>Reading freed memory normally lets the program keep running on corrupt data and crash later, far from the cause. Built with AddressSanitizer, the same access is checked against shadow memory and stops immediately with the exact file, line, and exit code.</desc>
<rect x="206" y="16" width="188" height="36" rx="5" fill="#D05663" fill-opacity="0.08" stroke="#D05663" stroke-opacity="0.6"/>
<text x="300" y="33" font-size="10.5" text-anchor="middle" fill="currentColor">read freed memory</text>
<text x="300" y="46" font-size="8.5" text-anchor="middle" fill="#D05663" opacity="0.9">(use-after-free)</text>
<line x1="250" y1="52" x2="160" y2="84" stroke="currentColor" stroke-opacity="0.4"/><path d="M163,77 L158,85 L167,84" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<line x1="350" y1="52" x2="442" y2="84" stroke="#12A594" stroke-opacity="0.6"/><path d="M435,82 L443,85 L438,77" fill="none" stroke="#12A594" stroke-opacity="0.7"/>
<text x="150" y="76" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.6">no sanitizer</text>
<rect x="44" y="86" width="216" height="34" rx="5" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.35"/><text x="152" y="107" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.75">keeps running on corrupt data</text>
<line x1="152" y1="120" x2="152" y2="140" stroke="currentColor" stroke-opacity="0.35"/><path d="M148,133 L152,141 L156,133" fill="none" stroke="currentColor" stroke-opacity="0.4"/>
<rect x="44" y="142" width="216" height="40" rx="5" fill="#D05663" fill-opacity="0.06" stroke="#D05663" stroke-opacity="0.45"/><text x="152" y="160" font-size="10" text-anchor="middle" fill="currentColor">crashes later — or not at all</text><text x="152" y="174" font-size="8.5" text-anchor="middle" fill="#D05663" opacity="0.85">far from the real cause</text>
<text x="450" y="76" font-size="9.5" text-anchor="middle" fill="#12A594" opacity="0.85" font-family="var(--md-code-font,monospace)">-fsanitize=address</text>
<rect x="340" y="86" width="220" height="34" rx="5" fill="#12A594" fill-opacity="0.08" stroke="#12A594" stroke-opacity="0.5"/><text x="450" y="107" font-size="10" text-anchor="middle" fill="currentColor">shadow memory checks each access</text>
<line x1="450" y1="120" x2="450" y2="140" stroke="#12A594" stroke-opacity="0.5"/><path d="M446,133 L450,141 L454,133" fill="none" stroke="#12A594" stroke-opacity="0.6"/>
<rect x="340" y="142" width="220" height="40" rx="5" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.2"/><text x="450" y="160" font-size="10" text-anchor="middle" fill="currentColor">STOP at the bad read</text><text x="450" y="174" font-size="8.5" text-anchor="middle" fill="#12A594" opacity="0.95" font-family="var(--md-code-font,monospace)">file:line · exit 134</text>
<text x="300" y="212" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">CI runs the suite twice — once normal, once under ASan (make test-asan)</text>
</svg>

<div class="lesson-widget" data-widget="asan-toggle-demo"></div>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **AddressSanitizer (ASan)** = "a watchdog baked into the compiled binary that screams the instant your code touches memory it shouldn't" — it works by painting freed or out-of-bounds memory with a poison marker, so when `ship_cleanup` reads `ship->cargo[i].dg` after `parse_cargo_list` already freed `ship->cargo`, ASan fires immediately instead of letting the program limp on with garbage data.
- **Undefined Behaviour Sanitizer (UBSan)** = "a trap on arithmetic and pointer operations that C normally lets silently go wrong" — things like signed integer overflow or a misaligned pointer read are technically undefined in C; UBSan turns them into an immediate, named abort rather than a mystery crash elsewhere.
- **Heap-use-after-free** = "reading memory you already handed back to the allocator" — the real bug in `parse_cargo_list` did exactly this: an error path freed `ship->cargo` but left `ship->cargo_count` non-zero, so `ship_cleanup` looped over the freed block; the fix was three lines — `free()`, set the pointer to `NULL`, reset the count.
- **Shadow memory** = "a hidden ledger ASan keeps alongside your real memory, one byte per eight bytes, recording whether each byte is live, freed, or never allocated" — every pointer read or write checks this ledger first; when it finds a poisoned byte the report is instant and pinpoints the exact source line.
- **`make test-asan` full clean rebuild** = "throw away all the old object files and recompile everything from scratch with sanitizer flags" — this matters because you cannot mix instrumented and un-instrumented `.o` files; one un-instrumented object silently breaks the shadow memory and the sanitizer may miss the bug it was there to catch.
- **Fuzzer + sanitizer pairing** = "throw random malformed input at a sanitized binary so bugs that only appear on unusual input get caught too" — `scripts/fuzz.sh` builds its own `-O1` ASan/UBSan binary and runs 300 iterations; exit code ≥ 128 (signal-crash) or any sanitizer diagnostic on stderr is a failure, enforcing the contract "never crash on bad input."

**Why it matters:** a heap-use-after-free in `parse_cargo_list` was completely invisible on normal runs — `free()` leaves memory readable for milliseconds, so the program exited cleanly with wrong data and no crash; ASan caught it on the first fuzz iteration and named the exact lines. Without sanitizers in CI, these bugs ship silently and corrupt stability calculations long before anyone notices a crash.

---

## What Sanitizers Are

C gives you full control of memory. The trade-off is that mistakes are silent by default: a buffer overrun may not crash immediately; a freed pointer may still hold readable data for milliseconds. The bug manifests far from its cause, if at all on a given run.

Sanitizers close that gap by turning the program into its own inspector. When you compile with ASan or UBSan, the compiler inserts tiny checks around every pointer dereference, every memory allocation, and every arithmetic operation. If a check fails, the program prints a detailed report and aborts — at the exact line of the violation.

Two sanitizers, two jobs:

| Sanitizer | What it instruments | Example bugs caught |
|-----------|--------------------|--------------------|
| **ASan** (AddressSanitizer) | Every heap allocation/free and every pointer read/write | Heap-use-after-free, buffer overflow, double-free, use of stack memory after return |
| **UBSan** (UndefinedBehaviourSanitizer) | Arithmetic, pointer arithmetic, type conversions | Signed integer overflow, null pointer dereference, misaligned memory access, out-of-bounds array index |

They are not separate programs — they are compiler flags. The overhead is real (roughly 2× slower, 2× more memory) but acceptable for a test run. You never ship a sanitizer binary to production.

---

## The `make test-asan` Target

In the [`Makefile`](https://github.com/alikatgh/CargoForge-C/blob/main/Makefile), the sanitized test run is a single target:

```makefile
# Makefile, lines 94–97
test-asan: CFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
test-asan: LDFLAGS += -fsanitize=address -fsanitize=undefined
test-asan: clean all test
	@echo "=== Memory safety tests (ASAN/UBSAN) passed ==="
```

Three flags are added to `CFLAGS` and mirrored in `LDFLAGS`:

- **`-fsanitize=address`** — enables ASan. The compiler adds shadow memory (one byte per 8 bytes of real memory) that records whether each byte is valid, freed, or never allocated. Every load and store checks this shadow before executing.
- **`-fsanitize=undefined`** — enables UBSan. Signed overflow, null deref, bad casts, and out-of-bounds pointer arithmetic all become fatal errors instead of silently producing garbage.
- **`-fno-omit-frame-pointer -g`** — keeps stack frames and debug symbols so that ASan's error report shows human-readable file names and line numbers rather than raw addresses.

Both flags must appear in **both** `CFLAGS` and `LDFLAGS`. The linker needs to pull in the sanitizer runtime libraries, not just the compiler. Forgetting the `LDFLAGS` side is a common mistake: the binary links fine but silently misses half the instrumentation.

The `clean` dependency is deliberate. The normal build uses `-O3` without debug symbols. If you just append flags to an incremental build, the old `.o` files are un-instrumented. `make test-asan` forces a full rebuild from source so every object file is compiled with the same flags.

!!! note
    You cannot mix instrumented and un-instrumented object files in the same binary. If even one `.o` is missing the ASan flags, the shadow memory is inconsistent and the sanitizer may miss bugs or crash the runtime itself.

---

## The Fuzzer and Sanitizer Integration

Running the eight unit tests under ASan covers the happy path and known edge cases. But the fuzzer ([`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) extends coverage to *inputs no human would write* — malformed configs, invalid DG strings, binary noise — precisely because parsers are the most common entry point for real-world crashes.

The fuzzer builds its own sanitized binary:

```bash
# scripts/fuzz.sh, lines 20–25
SAN=build/cargoforge-asan
mkdir -p build
if [ ! -x "$SAN" ]; then
    echo "Building sanitized binary ($SAN)..."
    cc -O1 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
       -fsanitize=address,undefined -fno-omit-frame-pointer src/*.c -lm -o "$SAN" || exit 1
fi
```

Note `-O1` instead of `-O3`. Full optimisation can obscure line numbers and rearrange code in ways that confuse sanitizer reports. `-O1` keeps enough optimisation to run at a useful speed while preserving readable stack traces.

The sanitizer environment variables are set before any run:

```bash
# scripts/fuzz.sh, lines 26–27
export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stacktrace=1"
```

- **`abort_on_error=1`**: instead of printing the report and continuing, the process aborts immediately on the first error. This produces an exit code of 134 (SIGABRT), which is ≥ 128 — the fuzzer's signal-crash threshold.
- **`halt_on_error=1`**: UBSan equivalent.
- **`print_stacktrace=1`**: UBSan includes a full stack trace in the report, not just the faulting line.

The pass/fail logic in the fuzzer is minimal:

```bash
# scripts/fuzz.sh, lines 77–84
rc=$?
if [ "$rc" -ge 128 ] || grep -qiE 'AddressSanitizer|runtime error:|UndefinedBehavior' "$err"; then
    echo "FUZZ FAIL (iter $i, sub=$sub, exit $rc):" >&2
    ...
    fail=1; break
fi
```

Exit code ≥ 128 means killed by a signal (crash). The `grep` catches any sanitizer diagnostic that leaked to stderr even on a non-signal exit. A clean rejection of bad input — any exit code below 128 with no sanitizer output — is a **pass**: the contract is "never crash on bad input," not "accept everything."

---

## Reading an ASan Report

When ASan finds a violation, it prints a structured report to stderr before aborting. Here is the shape of the report that the heap-use-after-free in `parse_cargo_list` produced:

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000001234 ...
READ of size 8 at 0x602000001234 thread T0
    #0 0x... in ship_cleanup src/analysis.c:211
    #1 0x... in dispatch_subcommand src/cli.c:88
    #2 0x... in main src/main.c:14

0x602000001234 was freed here:
    #0 0x... in parse_cargo_list src/parser.c:337

Previously allocated here:
    #0 0x... in parse_cargo_list src/parser.c:290
```

Three stacks, three facts:

1. **Where the bad read happened** — `ship_cleanup` in `analysis.c`, iterating `ship->cargo[i].dg`.
2. **Where the memory was freed** — an error path in `parse_cargo_list` that called `free(ship->cargo)`.
3. **Where it was originally allocated** — the array setup at the top of `parse_cargo_list`.

The report alone pinpoints the bug. Without ASan you would see a random crash (or no crash at all) long after the freed pointer was touched.

!!! tip
    Read the ASan report from the bottom of each stack upward — the bottom frame is the deepest library call, but the top frames are your code and are almost always the right place to start.

---

## What ASan Cannot Catch

ASan instruments heap and stack memory. It does not catch:

- **Logic errors** — computing the wrong GM formula produces a wrong answer, not a memory violation.
- **Uninitialised reads from stack memory** — use Valgrind (`make test-valgrind`) or `-fsanitize=memory` (MSan, Clang-only) for those.
- **Race conditions** — use ThreadSanitizer (`-fsanitize=thread`) for concurrent code. CargoForge-C is single-threaded, so this is not a current concern.

The `make test-valgrind` target in the Makefile runs the full test suite and an integration run through Valgrind with `--leak-check=full`, catching memory leaks that ASan can miss because the process exits cleanly while holding live allocations.

---

## The Real Bug That Sanitizers Found

The heap-use-after-free in `parse_cargo_list` is worth studying because it is invisible without sanitizers. When `safe_atof` rejected a weight field like `-5` or `abc`, the error path freed `ship->cargo` but left `ship->cargo_count` non-zero. Later, `ship_cleanup` iterated up to that count and read `ship->cargo[i].dg` — accessing the freed block.

On a typical run with no sanitizer, `free()` does not zero the memory immediately. The freed block still contains valid-looking pointer values. The access succeeds, `dg` reads as NULL or a plausible address, and the program exits cleanly. The bug is invisible.

Under ASan, the freed block is poisoned in the shadow memory the moment `free()` is called. The next read hits poisoned memory, ASan fires, the report names `parse_cargo_list` as the free site and `ship_cleanup` as the read site — and the fuzzer sees exit code 134.

The fix is three lines in `parser.c`:

```c
free(ship->cargo);
ship->cargo = NULL;    // prevent dangling pointer
ship->cargo_count = 0; // prevent iteration over freed memory
```

`ship_cleanup` checks `if (ship->cargo)` before entering the loop. After the fix, the NULL pointer makes the loop a no-op.

!!! warning
    The pattern "free a pointer, leave the count non-zero, let cleanup iterate" is the canonical heap-use-after-free shape in C. Any time you write an error-path `free()`, immediately set the pointer to NULL and reset the count.

---

## Recap

- ASan and UBSan are compiler flags (`-fsanitize=address,undefined`) that instrument every memory access and arithmetic operation and abort on the first violation with an exact stack trace.
- `make test-asan` does a **full clean rebuild** with those flags and then runs all eight test binaries — partial builds will miss bugs.
- The fuzzer ([`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) builds its own `-O1` sanitized binary and runs 300 iterations of random malformed input; exit code ≥ 128 or any sanitizer diagnostic on stderr is a failure.
- An ASan report gives three stacks: where the bad access happened, where the memory was freed, and where it was allocated — read them together to understand the ownership mistake.
- ASan does not catch uninitialised stack reads (use Valgrind/MSan) or logic errors; it is one layer of a multi-layer safety net.
- The real bug in `parse_cargo_list` — heap-use-after-free on the cargo array — was silent without sanitizers and immediately visible with them.

## Check yourself

??? question "How does AddressSanitizer actually detect a bad memory access?"
    It keeps a hidden shadow-memory ledger — one byte per eight real bytes — recording whether each byte is live, freed, or never allocated. Every read or write checks that ledger first; hitting a poisoned byte triggers an immediate, precisely located report.

??? question "Why can't you mix ASan-instrumented and plain object files in the same build?"
    One un-instrumented object file has no shadow-memory bookkeeping for its memory, which silently breaks the shadow-memory model for the whole binary. That's why `make test-asan` does a full clean rebuild rather than an incremental one.

*Next: [Static analysis](39-static-analysis.md).*
