# Coverage and benchmarks

Writing tests is one thing; knowing whether your tests exercise the code that actually matters is another. This lesson covers two complementary tools CargoForge-C uses to answer that question: `gcov` for line coverage and a purpose-built benchmark harness ([`validation/validate_benchmark.c`](https://github.com/alikatgh/CargoForge-C/blob/main/validation/validate_benchmark.c)) that validates the calculation engine against known good values for three vessel types, meeting the requirements of DNV-SE-0475 type approval.

## The mental model 🧠

Coverage answers "which lines did my tests actually run?" A green suite can lie: if no test drives the overweight error path, that path can be broken while every test still passes. `gcov` instruments the build (compiled at `-O0` so lines don't vanish into optimisation) to count how often each line runs, turning "tests pass" into "tests pass *and here is exactly what they touched*" — the untouched lines, marked `#####`, are your blind spots.

But coverage catches *silence*, not *lies*: a line can run and still compute the wrong GM. That is what the **validation harness** is for. `validate_benchmark.c` runs the real engine on three real vessels — a general cargo ship, a container ship, a bulk carrier — and checks hydrostatics, GZ, and IMO numbers against published stability-booklet values, to the tolerances DNV-SE-0475 type approval demands (absolute for tiny GZ areas, relative for huge displacements). Coverage shows *what you tested*; validation shows *whether the physics is right*.

<svg viewBox="0 0 600 200" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Coverage shows which lines the tests ran; validation checks the physics is right</title>
<desc>Coverage via gcov counts how often each line runs, exposing untested lines such as an error path even when every test passes. The validation harness runs the real engine on three reference vessels and checks the physics numbers against stability-booklet values within DNV-SE-0475 tolerances.</desc>
<text x="150" y="24" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.8">coverage — which lines ran?</text>
<rect x="24" y="32" width="252" height="120" rx="5" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.35"/>
<g font-family="var(--md-code-font,monospace)" font-size="9.5">
<circle cx="40" cy="50" r="3.5" fill="#12A594"/><text x="54" y="53" fill="currentColor" opacity="0.75">parse_cargo_list()</text>
<circle cx="40" cy="70" r="3.5" fill="#12A594"/><text x="54" y="73" fill="currentColor" opacity="0.75">place_cargo_3d()</text>
<circle cx="40" cy="90" r="3.5" fill="#12A594"/><text x="54" y="93" fill="currentColor" opacity="0.75">perform_analysis()</text>
<circle cx="40" cy="110" r="3.5" fill="#D05663"/><text x="54" y="113" fill="currentColor" opacity="0.6">error path (overweight)</text>
<circle cx="40" cy="130" r="3.5" fill="#12A594"/><text x="54" y="133" fill="currentColor" opacity="0.75">print_loading_plan()</text>
</g>
<text x="262" y="113" font-size="8" text-anchor="end" fill="#D05663" opacity="0.85">never run</text>
<text x="150" y="170" font-size="9" text-anchor="middle" fill="currentColor" opacity="0.65">87% of lines — the red line is a blind spot</text>
<text x="450" y="24" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.8">validation — is the physics right?</text>
<rect x="324" y="32" width="252" height="120" rx="5" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.35"/>
<g font-family="var(--md-code-font,monospace)" font-size="9.5">
<text x="338" y="52" fill="currentColor" opacity="0.5">field     ref     got</text>
<text x="338" y="74" fill="currentColor" opacity="0.8">GM        1.39    1.39</text><text x="562" y="74" fill="#12A594" text-anchor="end">✓</text>
<text x="338" y="96" fill="currentColor" opacity="0.8">displ     42550   42490</text><text x="562" y="96" fill="#12A594" text-anchor="end">✓</text>
<text x="338" y="118" fill="currentColor" opacity="0.8">GZ@30     0.21    0.21</text><text x="562" y="118" fill="#12A594" text-anchor="end">✓</text>
</g>
<text x="450" y="142" font-size="8" text-anchor="middle" fill="currentColor" opacity="0.55">3 reference vessels · stability-booklet values · DNV-SE-0475</text>
<text x="300" y="190" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.7">coverage shows what you tested · validation shows whether it's right</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Line coverage** = "a count of which source lines the CPU actually ran during your tests" — `gcov` instruments the compiled object files so that every executed line increments a counter; unexecuted lines show up as `#####` in the `.gcov` report, telling you exactly where your tests have blind spots.
- **`-O0` when building for coverage** = "turn off the compiler's shortcuts so every line stays visible" — optimisation can inline or merge lines until they vanish from the coverage map, making the report lie; `-O0` keeps the source and the binary in one-to-one correspondence.
- **Coverage as a gap-finder, not a quality certificate** = "knowing a line ran is not the same as knowing it ran correctly" — `perform_analysis` could execute every branch of `hydro_interpolate` and still return a wrong GM if the interpolation arithmetic has a sign error; coverage catches the silence, not the lie.
- **`validate_benchmark.c` / `make validate`** = "run the real engine on three real ships and check the physics" — unlike unit tests that feed synthetic inputs to isolated functions, the benchmark harness calls `perform_analysis` on a general cargo ship, a container ship, and a bulk carrier, then compares hydrostatics, GZ values, and IMO criteria against stability-booklet reference values required by DNV-SE-0475 type approval.
- **`CHECK_ABS` vs `CHECK_REL` tolerance modes** = "pick the ruler that fits the size of the number" — a GZ area of 0.055 m·rad is checked with an absolute tolerance (±0.001 m·rad) because the number is inherently small; a displacement of 42,550 tonnes is checked with a relative tolerance (0.5%) because small absolute differences are meaningless at that scale.
- **Manually positioned cargo in the harness (`add_cargo`)** = "fix the inputs so the benchmark never silently changes" — instead of calling the 3D bin-packer, the harness sets `pos_x`, `pos_y`, `pos_z` directly (mirroring what `parse_cargo_list` stores in the `Cargo` struct); if the packer's heuristic ever changes, the reference numbers stay stable.
- **Wall-sided formula cross-check (Test 4)** = "recompute GZ independently in the test and demand the engine agrees" — the harness calculates $GZ(\theta)=\sin\theta(GM + BM\tan^2\theta/2)$ itself and asserts it matches `r.gz_at_30` from `perform_analysis` to within 0.01 m, catching unit or sign errors in `gz_at_angle` without needing an external reference value.

**Why it matters:** if coverage gaps go unnoticed, error-handling paths like the malformed-DG branch in `parse_dg_field` are never exercised and can silently misbehave in production; if the benchmark isn't run in CI, a change to `hydrostatics.c` or `analysis.c` can drift outside physical tolerance and break DNV type-approval compliance before anyone notices.

---

## What is line coverage?

Every time the CPU executes a line of your program, a coverage tool can record that fact. After a full test run, you get a report: which lines were reached and how many times, which lines were never reached at all.

`gcov` is the coverage tool that ships with GCC. It works by instrumenting your object files at compile time: the compiler inserts counters beside each line, and after execution those counters are written to `.gcda` files on disk. `gcov` reads those files and produces annotated source listings.

### Building for coverage

The standard CargoForge-C build flags are:

```makefile
CFLAGS = -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
```

To add coverage, you rebuild with two extra flags — `--coverage` at both compile and link time:

```bash
make clean
CFLAGS="--coverage -O0 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude" \
LDFLAGS="--coverage -lm" make test
```

`-O0` turns off optimisation. Optimisation can merge or eliminate lines in ways that make coverage reports misleading — an inlined function body may disappear entirely from the coverage map.

After the test run completes, `.gcda` files appear alongside the `.o` files in `build/`. To get a human-readable report for a single source file:

```bash
gcov -o build/ src/analysis.c
```

This prints a summary and creates `analysis.c.gcov`. Each line is prefixed with a hit count or `#####` for unexecuted lines. For a project-wide HTML report, `lcov` and `genhtml` can aggregate all the `.gcda` files into a browsable tree.

## What coverage tells you — and what it does not

Coverage is a **necessary condition for confidence, not a sufficient one**.

A line that is never executed cannot be correct. If your error-handling branch for a malformed DG field (`parse_dg_field` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)) is never reached by any test, you have no evidence it works. Coverage gives you that signal cheaply.

What coverage cannot tell you:

- **Correctness of the values produced.** A line can execute a thousand times with wrong results. The counter only counts; it does not check.
- **That all interesting inputs were tried.** Reaching the `hydro_interpolate` function once, with a draft that falls exactly on a table entry, tells you nothing about what happens when the draft falls between rows, or above the table's maximum, or below its minimum.
- **Concurrency or ordering bugs.** The server ([`src/server.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/server.c)) handles one connection at a time, but even single-threaded timing-sensitive code can misbehave in ways no coverage tool detects.

The rule in CargoForge-C: use coverage to find gaps, then write tests targeted at those gaps. Do not treat a 90% coverage number as a quality certificate.

!!! note
    The Makefile's `test-asan` target compiles with `-fsanitize=address -fsanitize=undefined`, which catches a different class of bug (memory corruption, signed overflow, null dereference) than coverage identifies. Run both: coverage tells you which code was reached; ASan tells you whether that code misbehaved while running.

## The benchmark validation harness

Unit tests verify that individual functions return expected values for synthesised inputs. The benchmark harness in [`validation/validate_benchmark.c`](https://github.com/alikatgh/CargoForge-C/blob/main/validation/validate_benchmark.c) does something different: it constructs realistic loading conditions for three real vessel types, calls `perform_analysis` (the same function the production CLI uses), and checks the results against stability-booklet reference values.

This kind of validation is required for DNV-SE-0475 type approval — a class society process that certifies software used aboard ships for regulatory calculations. The header comment in [`validation/validate_benchmark.c`](https://github.com/alikatgh/CargoForge-C/blob/main/validation/validate_benchmark.c) states the goal explicitly:

```c
/*
 * Validates CargoForge calculation engine against known stability booklet
 * values for three benchmark vessel types. This is a key deliverable for
 * DNV Type Approval under DNV-SE-0475 (Computer Software).
 *
 * Approach: Manually position cargo for controlled loading conditions
 * (as per stability booklet test cases), then validate the calculation
 * engine's output against expected values.
 *
 * Build:   make validate
 * Run:     ./validation/validate_benchmark
 */
```

The `make validate` target in the Makefile links the harness against the core engine objects (no CLI, no `main.c`) and runs it:

```makefile
VALIDATE_OBJS = $(BUILD_DIR)/parser.o $(BUILD_DIR)/analysis.o \
                $(BUILD_DIR)/placement_3d.o $(BUILD_DIR)/constraints.o \
                $(BUILD_DIR)/hydrostatics.o $(BUILD_DIR)/tanks.o \
                $(BUILD_DIR)/longitudinal_strength.o $(BUILD_DIR)/imdg.o

validate: $(BUILD_DIR) validation/validate_benchmark
	@echo "--- Running Benchmark Vessel Validation ---"
	./validation/validate_benchmark
	@echo "--- Validation Complete ---"

validation/validate_benchmark: validation/validate_benchmark.c $(HDRS) $(VALIDATE_OBJS)
	$(CC) $(CFLAGS) -o $@ validation/validate_benchmark.c $(VALIDATE_OBJS) $(LDFLAGS)
```

`libcargoforge.c` is intentionally excluded. The harness calls internal engine functions directly so it can test subsystems individually as well as the full pipeline.

### The test framework

The harness has its own small assertion framework — about 25 lines of C:

```c
static int total_checks = 0;
static int passed_checks = 0;
static int failed_checks = 0;

typedef enum { TOL_ABSOLUTE, TOL_RELATIVE } ToleranceType;

static int check_value(const char *param, float actual, float expected,
                       float tolerance, ToleranceType tol_type) {
    total_checks++;
    float error;
    const char *unit;

    if (tol_type == TOL_RELATIVE && fabsf(expected) >= 1e-6f) {
        error = fabsf(actual - expected) / fabsf(expected) * 100.0f;
        unit = "%";
    } else {
        error = fabsf(actual - expected);
        unit = "";
    }

    int pass = (error < tolerance);
    if (pass) { passed_checks++; /* ... */ }
    else       { failed_checks++; /* ... */ }
    return pass;
}
```

`CHECK_ABS(param, actual, expected, tolerance)` checks that `|actual − expected| < tolerance`. `CHECK_REL` checks that the relative error is below a percentage threshold. Physical quantities that span orders of magnitude (tank weight in tonnes versus a GZ area in m·rad) each use the appropriate form.

The reason for two tolerance modes matters: a GZ area of 0.055 m·rad is a small absolute number, so an absolute tolerance of 0.001 m·rad is meaningful. A displacement of 42,550 tonnes has natural variability in interpolation, so a 1-tonne absolute tolerance is finer than needed — a 0.5% relative tolerance is a better expression of the acceptable error.

### The three benchmark vessels

| Vessel | Length | Beam | Lightship weight |
|---|---|---|---|
| General cargo ship | 112 m | 18.6 m | 3,200 t |
| Container ship | 283 m | 32.2 m | 18,500 t |
| Bulk carrier | 190 m | 32.26 m | 9,800 t |

Each vessel has a real hydrostatic CSV table in `validation/vessels/<type>/hydro.csv`, a tank CSV, and (for the general cargo ship) explicit permissible strength limits. The harness loads these files using the same `parse_hydro_table` and `parse_tank_config` functions used by the production parser, so the validation covers the I/O layer as well as the maths.

Cargo is positioned manually using the `add_cargo` helper, which sets `pos_x`, `pos_y`, `pos_z` directly rather than invoking the 3D bin-packer. This is deliberate: benchmarks must be **deterministic and repeatable**. If the packer's heuristic ever changes, the benchmark should not silently change too.

```c
static void add_cargo(Ship *ship, const char *id, float weight_t,
                      float l, float w, float h,
                      float px, float py, float pz) {
    Cargo *c = &ship->cargo[ship->cargo_count];
    memset(c, 0, sizeof(*c));
    strncpy(c->id, id, sizeof(c->id) - 1);
    strcpy(c->type, "standard");
    c->weight = weight_t * 1000.0f;   /* tonnes → kg */
    c->dimensions[0] = l;
    c->dimensions[1] = w;
    c->dimensions[2] = h;
    c->pos_x = px; c->pos_y = py; c->pos_z = pz;
    ship->cargo_count++;
}
```

Notice `weight_t * 1000.0f`. The `Cargo.weight` field always stores kilograms; the helper takes the human-readable tonne value and converts it, matching the behaviour of `parse_cargo_list`.

### What the nine tests cover

| Test | Subsystem exercised |
|---|---|
| 1 — Hydrostatic table interpolation | `hydro_interpolate`, `hydro_draft_from_displacement`, boundary clamping, all three vessel tables |
| 2 — Free surface correction | `calculate_total_fsm`, `calculate_virtual_kg_rise`, `calculate_tank_weight` |
| 3 — Longitudinal strength | `calculate_longitudinal_strength`, `check_strength_limits`, boundary values SF=0 at AP |
| 4 — GZ curve | Wall-sided formula identity check: computed `gz_at_30` must equal $\sin(30°)(GM + BM\tan^2(30°)/2)$ to within 0.01 m |
| 5–7 — Full stability (one per vessel type) | Full pipeline: `perform_analysis` with real hydro + tanks + strength limits; all six IMO criteria |
| 8 — IMDG compliance | Segregation matrix lookups, minimum distances, `imdg_check_all` on a mixed DG/non-DG manifest |
| 9 — Cross-validation | Same loading condition analysed with table hydrostatics vs box-hull fallback; GM sign and draft ratio checked |

Test 4 is worth examining closely. It does not just check that `gz_at_30` is positive — it recomputes the wall-sided formula independently in the test and asserts the two values agree:

```c
float theta = 30.0f * (float)M_PI / 180.0f;
float tan_t = tanf(theta);
float exp_gz30 = sinf(theta) * (gm + bm * tan_t * tan_t / 2.0f);

CHECK_ABS("GZ@30 matches formula", r.gz_at_30, exp_gz30, 0.01f);
```

$$GZ(\theta) = \sin\theta \left(GM + \frac{BM \tan^2\theta}{2}\right)$$

This is the wall-sided formula implemented in `gz_at_angle` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c). The test is checking internal consistency — if someone accidentally introduced a sign error or a unit conversion mistake, this check catches it even without an external reference value.

### Summary output and exit code

At the end of `main`, the harness prints a table of totals and exits non-zero if any check failed:

```c
if (failed_checks > 0) {
    printf("\n  *** VALIDATION FAILED: %d check(s) ***\n\n", failed_checks);
    return 1;
}
printf("\n  ALL CHECKS PASSED - Validation successful\n\n");
return 0;
```

`make validate` therefore fails the build (via Make's error propagation) if the calculation engine drifts outside tolerance. You can add this target to a CI pipeline alongside `make test` and `make test-asan` to get three distinct layers of assurance: unit correctness, memory safety, and physics accuracy.

## Coverage and benchmarks together

Think of the three layers as answering different questions:

| Tool | Question answered |
|---|---|
| `make test` | Do individual functions return expected values for hand-crafted inputs? |
| `gcov` | Are there code paths the tests never reach? |
| `make test-asan` | Does the code crash or corrupt memory on the inputs the tests exercise? |
| `make validate` | Does the engine produce physically plausible results on realistic vessel configurations? |

None of these layers replaces the others. A benchmark can pass even if there is an uncovered error path that only triggers on malformed input. Coverage can be 100% even if every line produces a wrong value. ASan can be clean even if the logic is subtly wrong. Running all four layers together is what makes the test suite meaningful rather than merely reassuring.

## Recap

- `gcov` counts which source lines execute during a test run; build with `--coverage` and `-O0`, then run your tests and call `gcov` on the source files.
- Coverage identifies unreached code but cannot verify correctness of the values produced — it is a gap-finder, not a quality certificate.
- The `validate_benchmark.c` harness positions cargo manually on three benchmark vessels (general cargo, container ship, bulk carrier) and calls `perform_analysis` directly to check that hydrostatics, GZ values, and IMO criteria fall within tolerance of known reference values.
- Two tolerance modes — `CHECK_ABS` for quantities with small natural values (GZ, GM), `CHECK_REL` for large quantities (displacement, tank weight) — make the acceptance thresholds physically meaningful.
- Test 4 cross-checks `gz_at_30` against the wall-sided formula computed independently in the test itself, catching unit or sign errors without needing an external reference.
- `make validate` exits non-zero if any check fails, making physics accuracy a first-class CI gate alongside memory safety (`make test-asan`) and unit correctness (`make test`).

## Check yourself

??? question "Why must coverage measurement build with -O0 instead of the normal optimised build?"
    Optimisation can inline or merge lines until they vanish from the coverage map entirely, making the report lie about what actually ran. `-O0` keeps a one-to-one correspondence between the source lines you read and the binary that executes.

??? question "A function can have 100% line coverage and still return a wrong stability number. Why doesn't coverage catch that?"
    Coverage only proves a line *executed*, not that it computed the right value. A sign error in an interpolation formula could run on every test case and still silently return the wrong number — only comparing against known-good reference values, the way `validate_benchmark.c` does, catches that.

*Next: [Lab 10 — The Quality Gate](lab-10-quality.md).*
