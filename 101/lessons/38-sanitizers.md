# Sanitizers (ASan/UBSan)

Unit tests tell you what your program *should* do. Sanitizers tell you what it *is* doing with memory at runtime. AddressSanitizer (ASan) and Undefined Behaviour Sanitizer (UBSan) are compiler-level tools that instrument every memory access and arithmetic operation and report violations the moment they occur — not after the program silently corrupts data downstream. CargoForge-C uses both as a mandatory gate in its CI pipeline, and they caught the real heap-use-after-free bug that lived in `parse_cargo_list`.

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

## The mental model 🧠

You'll forget the flags — hold THIS picture instead:

> Imagine a ship's cargo hold where every shelf is painted with invisible ink the moment its goods are unloaded. A dock worker (your code) reaches back onto that empty, "poisoned" shelf to grab something that's already gone. A watchman standing right there — with a UV lamp — screams instantly, points at the worker, and names the exact shelf. That's ASan.

In CargoForge-C terms: `parse_cargo_list` built the cargo array and eventually called `free(ship->cargo)` — the shelf was unloaded. The poison went on immediately. Then `ship_cleanup` walked the same shelf (`ship->cargo[i].dg`) because `ship->cargo_count` still said it was full. The watchman (ASan's shadow memory) lit it up the moment the loop touched the poisoned byte, printed three stacks — free site, read site, allocation site — and aborted. Without the UV lamp, the freed memory still looked readable for milliseconds and the bug was completely invisible.

The watchman doesn't know what the cargo *means* (logic errors are yours to find), only whether the shelf is live or poisoned. That one guarantee turns silent corruption into an instant, named abort at the exact source line.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>ASan catching heap-use-after-free in CargoForge-C</title>
  <desc>Three-phase diagram: parse_cargo_list allocates the cargo array (green), an error path frees it and poisons shadow memory (red), then ship_cleanup tries to read the freed block and ASan fires immediately.</desc>

  <!-- Phase labels -->
  <text x="90" y="22" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor" opacity="0.55">① allocate</text>
  <text x="310" y="22" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor" opacity="0.55">② free + poison</text>
  <text x="530" y="22" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor" opacity="0.55">③ bad read → ABORT</text>

  <!-- Phase ①: cargo array allocated -->
  <!-- box: cargo array -->
  <rect x="20" y="36" width="140" height="56" rx="5" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="90" y="55" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">ship-&gt;cargo[ ]</text>
  <text x="90" y="72" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">malloc'd in</text>
  <text x="90" y="85" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">parse_cargo_list</text>
  <!-- shadow memory: all OK -->
  <rect x="20" y="104" width="140" height="30" rx="4" fill="#12A594" opacity="0.15" stroke="#12A594" stroke-width="1.2"/>
  <text x="90" y="124" text-anchor="middle" font-size="10" fill="#12A594" font-weight="600">shadow: LIVE ✓</text>

  <!-- arrow ①→② -->
  <line x1="165" y1="85" x2="205" y2="85" stroke="currentColor" stroke-width="1.4" opacity="0.5"/>
  <polygon points="205,81 213,85 205,89" fill="currentColor" opacity="0.5"/>
  <text x="189" y="78" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.45">error path</text>

  <!-- Phase ②: freed + shadow poisoned -->
  <rect x="218" y="36" width="184" height="56" rx="5" fill="none" stroke="#D05663" stroke-width="1.8"/>
  <text x="310" y="55" text-anchor="middle" font-size="11" font-weight="700" fill="#D05663">free(ship-&gt;cargo)</text>
  <text x="310" y="72" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">cargo_count left non-zero</text>
  <text x="310" y="85" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">parser.c:337</text>
  <!-- shadow memory: poisoned -->
  <rect x="218" y="104" width="184" height="30" rx="4" fill="#D05663" opacity="0.15" stroke="#D05663" stroke-width="1.2"/>
  <text x="310" y="124" text-anchor="middle" font-size="10" fill="#D05663" font-weight="600">shadow: POISONED ✗</text>

  <!-- arrow ②→③ -->
  <line x1="407" y1="85" x2="447" y2="85" stroke="currentColor" stroke-width="1.4" opacity="0.5"/>
  <polygon points="447,81 455,85 447,89" fill="currentColor" opacity="0.5"/>
  <text x="431" y="78" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.45">cleanup runs</text>

  <!-- Phase ③: ship_cleanup reads freed block — ASan fires -->
  <rect x="460" y="36" width="140" height="56" rx="5" fill="none" stroke="currentColor" stroke-width="1.4" opacity="0.7"/>
  <text x="530" y="52" text-anchor="middle" font-size="10" font-weight="700" fill="currentColor">ship_cleanup</text>
  <text x="530" y="66" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">reads cargo[i].dg</text>
  <text x="530" y="80" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">analysis.c:211</text>
  <!-- ASan ABORT badge -->
  <rect x="466" y="104" width="128" height="30" rx="4" fill="#D05663" opacity="0.9"/>
  <text x="530" y="124" text-anchor="middle" font-size="10" fill="#fff" font-weight="700">ASan ABORT ✗</text>

  <!-- separator -->
  <line x1="20" y1="152" x2="600" y2="152" stroke="currentColor" stroke-width="0.8" opacity="0.18" stroke-dasharray="4,4"/>

  <!-- Report box -->
  <rect x="20" y="164" width="580" height="96" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <text x="36" y="182" font-size="10" font-weight="700" fill="#D05663">ERROR: AddressSanitizer: heap-use-after-free</text>
  <text x="36" y="198" font-size="10" fill="currentColor" opacity="0.75">READ of size 8 …  →  #0 ship_cleanup  analysis.c:211   (where the bad read happened)</text>
  <text x="36" y="214" font-size="10" fill="currentColor" opacity="0.75">freed here       →  #0 parse_cargo_list  parser.c:337     (where free() was called)</text>
  <text x="36" y="230" font-size="10" fill="currentColor" opacity="0.75">allocated here   →  #0 parse_cargo_list  parser.c:290     (where malloc was called)</text>
  <text x="36" y="250" font-size="10" fill="#12A594" font-weight="600">Fix: free(ship-&gt;cargo);  ship-&gt;cargo = NULL;  ship-&gt;cargo_count = 0;</text>
</svg>

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

*Next: [Static analysis](39-static-analysis.md).*
