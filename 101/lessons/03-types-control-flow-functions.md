# Types, control flow, and functions

C is a typed language: every variable has a fixed type declared at compile time, and the type determines how memory is used and what operations are legal. Understanding why CargoForge-C chooses `float` over `double` for physics, and how functions isolate that physics into reusable units, is the first step to reading the codebase fluently.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Type** = "a label the compiler uses to know how big a variable is and what you can do with it" — in CargoForge-C this is why every draught, weight, and angle is `float` (4 bytes, enough for six digits) rather than the larger `double`, and why the `Ship` and `Cargo` structs stay compact and cache-friendly.
- **`float` vs `double`** = "single-precision vs double-precision decimal numbers — about 6–7 digits of accuracy vs 15–16" — real sea-going instruments (draught gauges, load cells) don't resolve beyond a few centimetres or half a tonne, so `float` matches the sensor, and the `f` suffix on every constant like `1.025f` in `analysis.c` tells the compiler to keep it that way.
- **`memset` zero-init** = "wiping a struct's memory to all zeros before filling it in" — `perform_analysis` does `memset(&r, 0, sizeof(r))` on the `AnalysisResult` before computing anything, because C does not automatically zero local variables and reading uninitialised memory is undefined behaviour.
- **`static` on a function** = "this function is private to this one `.c` file — nothing else can call it" — `gz_at_angle`, `integrate_gz`, and `find_gz_max` are all marked `static` in `analysis.c` so they stay hidden implementation details; only `perform_analysis` and `print_loading_plan` are visible to the rest of the program.
- **Pass by value** = "the function gets its own copy of whatever you hand it" — when `perform_analysis` calls `gz_at_angle(gm_effective, r.bm, 30.0f)`, the function works on local copies of those numbers and cannot accidentally change the caller's variables; to write a result back, a pointer (like `float *max_gz` in `find_gz_max`) is passed instead.
- **Trapezoidal rule in `integrate_gz`** = "slicing the GZ stability curve into 100 thin trapezoids and summing their areas to approximate the area under the curve" — the `for` loop in `integrate_gz` does exactly this, and the `if (gz < 0) gz = 0` guards inside it ensure a negative righting lever never artificially inflates the stability area.

**Why it matters:** if you mix up `float` and `double` (missing the `f` suffix) the compiler silently promotes constants to `double` and back, wasting cycles in the physics hot path; if you read an uninitialised field in an `AnalysisResult` you get garbage GM or KB values that could pass safety checks with nonsense numbers.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> Imagine a ship's engine room where each dial is labelled with a unit: one says *draught (m)*, another *GM (m)*, another *heel angle (°)*. The labels are the **types**. A mechanic who accidentally reads the draught dial as an angle will get nonsense — the number looks the same but means something completely different. The compiler plays the role of that mechanic's supervisor, refusing to mix the dials at build time.

In CargoForge-C every physics quantity is a `float` dial — `gm`, `bm`, `theta_deg` in `gz_at_angle`. The **function** is the instrument panel itself: you hand it three readings (by value — your own dials are untouched), it does one well-defined calculation (the wall-sided GZ formula), and it gives back a single result. The `static` label on the door means only the engine room (`analysis.c`) may enter. `perform_analysis` is the chief engineer who orchestrates all the panels: it zero-initialises the `AnalysisResult` logbook with `memset`, then calls each instrument panel in turn, writing results back through pointer parameters like `&r.gz_max`.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Function call chain in analysis.c</title>
  <desc>Diagram showing perform_analysis calling gz_at_angle, integrate_gz, and find_gz_max, with float parameters and return values annotated for each call.</desc>

  <!-- perform_analysis box (caller) -->
  <rect x="10" y="110" width="168" height="90" rx="6"
        fill="none" stroke="#12A594" stroke-width="2"/>
  <text x="94" y="133" text-anchor="middle" font-size="12" font-weight="700"
        fill="#12A594">perform_analysis</text>
  <text x="94" y="152" text-anchor="middle" font-size="10"
        fill="currentColor" opacity="0.75">const Ship *ship</text>
  <text x="94" y="167" text-anchor="middle" font-size="10"
        fill="currentColor" opacity="0.75">AnalysisResult r</text>
  <text x="94" y="182" text-anchor="middle" font-size="10"
        fill="currentColor" opacity="0.75">memset(&amp;r, 0, sizeof(r))</text>

  <!-- Arrow to gz_at_angle -->
  <line x1="178" y1="136" x2="240" y2="65" stroke="currentColor" stroke-width="1.5"
        opacity="0.6" marker-end="url(#arr)"/>
  <!-- call label -->
  <text x="196" y="96" font-size="9" fill="currentColor" opacity="0.7">float gm, bm, 30.0f</text>
  <!-- return label -->
  <text x="196" y="108" font-size="9" fill="#12A594">→ float GZ</text>

  <!-- gz_at_angle box -->
  <rect x="240" y="16" width="168" height="76" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.85"/>
  <text x="324" y="36" text-anchor="middle" font-size="11" font-weight="700"
        fill="currentColor">gz_at_angle</text>
  <text x="324" y="53" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">static float</text>
  <text x="324" y="67" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">params: gm, bm, theta_deg</text>
  <text x="324" y="81" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">return sinf(θ)·(gm + bm·tan²θ/2)</text>

  <!-- Arrow to integrate_gz -->
  <line x1="178" y1="155" x2="240" y2="163" stroke="currentColor" stroke-width="1.5"
        opacity="0.6" marker-end="url(#arr)"/>
  <text x="188" y="152" font-size="9" fill="currentColor" opacity="0.7">gm, bm, 0°…90°</text>
  <text x="188" y="163" font-size="9" fill="#12A594">→ float area</text>

  <!-- integrate_gz box -->
  <rect x="240" y="128" width="168" height="76" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.85"/>
  <text x="324" y="148" text-anchor="middle" font-size="11" font-weight="700"
        fill="currentColor">integrate_gz</text>
  <text x="324" y="165" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">static float</text>
  <text x="324" y="179" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">for i in 0..100: trapezoid</text>
  <text x="324" y="193" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">if gz &lt; 0: clamp to 0</text>

  <!-- Arrow to find_gz_max -->
  <line x1="178" y1="178" x2="240" y2="238" stroke="currentColor" stroke-width="1.5"
        opacity="0.6" marker-end="url(#arr)"/>
  <text x="182" y="214" font-size="9" fill="currentColor" opacity="0.7">gm, bm,</text>
  <text x="182" y="225" font-size="9" fill="currentColor" opacity="0.7">&amp;r.gz_max, &amp;r.gz_max_angle</text>
  <text x="182" y="236" font-size="9" fill="#D05663">pointer out-params</text>

  <!-- find_gz_max box -->
  <rect x="240" y="218" width="168" height="76" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.85"/>
  <text x="324" y="238" text-anchor="middle" font-size="11" font-weight="700"
        fill="currentColor">find_gz_max</text>
  <text x="324" y="255" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">static void</text>
  <text x="324" y="269" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.7">for a 1°..80°: scan peak</text>
  <text x="324" y="283" text-anchor="middle" font-size="9"
        fill="#D05663">writes *max_gz, *max_angle</text>

  <!-- AnalysisResult r label on right -->
  <rect x="430" y="110" width="176" height="90" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"
        stroke-dasharray="5,3"/>
  <text x="518" y="130" text-anchor="middle" font-size="11" font-weight="600"
        fill="currentColor" opacity="0.8">AnalysisResult r</text>
  <text x="518" y="148" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.65">r.gz_at_30  ← return value</text>
  <text x="518" y="162" text-anchor="middle" font-size="9"
        fill="currentColor" opacity="0.65">r.stability_area ← return value</text>
  <text x="518" y="176" text-anchor="middle" font-size="9"
        fill="#D05663" opacity="0.9">r.gz_max      ← *pointer write</text>
  <text x="518" y="190" text-anchor="middle" font-size="9"
        fill="#D05663" opacity="0.9">r.gz_max_angle ← *pointer write</text>

  <!-- Arrow from integrate_gz result to r -->
  <line x1="408" y1="166" x2="428" y2="160" stroke="#12A594" stroke-width="1.2"
        opacity="0.7" marker-end="url(#arr-teal)"/>
  <!-- Arrow from find_gz_max result to r -->
  <line x1="408" y1="256" x2="517" y2="197" stroke="#D05663" stroke-width="1.2"
        opacity="0.7" marker-end="url(#arr-red)"/>

  <!-- Legend -->
  <line x1="12" y1="288" x2="36" y2="288" stroke="#12A594" stroke-width="2"/>
  <text x="40" y="292" font-size="9" fill="currentColor" opacity="0.75">return value (by value)</text>
  <line x1="150" y1="288" x2="174" y2="288" stroke="#D05663" stroke-width="2"/>
  <text x="178" y="292" font-size="9" fill="currentColor" opacity="0.75">pointer out-param (&amp;r.field)</text>
  <text x="380" y="292" font-size="9" fill="currentColor" opacity="0.55">all functions: static (private to analysis.c)</text>

  <!-- Arrow markers -->
  <defs>
    <marker id="arr" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L8,3 z" fill="currentColor" opacity="0.6"/>
    </marker>
    <marker id="arr-teal" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L8,3 z" fill="#12A594" opacity="0.8"/>
    </marker>
    <marker id="arr-red" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L8,3 z" fill="#D05663" opacity="0.8"/>
    </marker>
  </defs>
</svg>

## Primitive types

C has a small set of numeric types that map directly to CPU registers.

| Type | Typical size | Range / precision | Use in CargoForge-C |
|------|-------------|-------------------|---------------------|
| `int` | 4 bytes | −2.1 × 10⁹ to +2.1 × 10⁹ | loop counters, item counts, flag values |
| `float` | 4 bytes | ≈ 6–7 significant digits | all physics quantities (metres, kg, degrees) |
| `double` | 8 bytes | ≈ 15–16 significant digits | trigonometry return values, rarely used directly |
| `char` | 1 byte | 0–255 | characters; arrays of `char` form strings |

### Why `float` for physics, not `double`?

Stability calculations involve measurements made at sea: draughts measured to the nearest centimetre, weights accurate to perhaps half a tonne. Six significant digits of precision is more than enough — a real draught gauge does not resolve below a millimetre. Using `float` instead of `double`:

- Halves the memory footprint of the `Cargo` and `Ship` structs (dozens of `float` fields vs `double`).
- Keeps struct sizes predictable and cache-friendly.
- Matches the precision of the physical sensors that produced the input data.

`analysis.c` mixes both: the physics stays in `float`, but the `<math.h>` functions (`sinf`, `cosf`, `tanf`, `atanf`) are the single-precision variants explicitly, so no hidden conversion to `double` and back happens in the hot path.

!!! note "The `f` suffix"
    In C, a bare literal like `3.14` is a `double`. Write `3.14f` to get a `float` literal. You will see this throughout `analysis.c`:
    ```c
    #define SEAWATER_DENSITY  1.025f
    #define KB_FACTOR         0.53f
    ```
    Without the `f`, the compiler silently promotes the constant to `double` for any arithmetic that touches it, then truncates it back — wasted work and a subtle precision trap.

---

## Declaring variables

The pattern in C is always: **type, then name, then optional initialiser**.

```c
int   steps = 100;
float area  = 0.0f;
float theta_deg;       /* declared but not yet initialised */
```

C does not zero-initialise local variables automatically. Reading an uninitialised variable is undefined behaviour. CargoForge-C handles this by initialising every local it uses before reading it — and by using `memset` to zero entire structs before filling them in:

```c
/* from perform_analysis, analysis.c:90 */
AnalysisResult r;
memset(&r, 0, sizeof(r));
```

---

## Control flow

### `if` / `else if` / `else`

The body of an `if` can be a single statement or a block delimited by `{` `}`. CargoForge-C uses blocks consistently.

From `print_loading_plan` in `analysis.c`:

```c
const char *gm_str;
float gm_display = a.gm_corrected;
if (gm_display < 0.3f)
    gm_str = "CRITICAL - Too tender";
else if (gm_display > 3.0f)
    gm_str = "WARNING - Too stiff";
else if (gm_display >= 0.5f && gm_display <= 2.5f)
    gm_str = "Optimal";
else
    gm_str = "Acceptable";
```

The conditions are evaluated top-to-bottom; only the first matching branch runs. The `&&` operator is logical AND — both sub-conditions must be true for the branch to be taken.

### `for` loops

```
for ( initialiser ; condition ; update ) { body }
```

The initialiser runs once. Before each iteration the condition is checked; if false, the loop stops. The update runs at the end of each iteration.

From `integrate_gz` in `analysis.c`:

```c
int steps = 100;
float step = (end_deg - start_deg) / steps;
float area = 0.0f;

for (int i = 0; i < steps; i++) {
    float a1 = start_deg + i * step;
    float a2 = a1 + step;
    float gz1 = gz_at_angle(gm, bm, a1);
    float gz2 = gz_at_angle(gm, bm, a2);
    if (gz1 < 0) gz1 = 0;
    if (gz2 < 0) gz2 = 0;
    float da = step * (float)M_PI / 180.0f;
    area += (gz1 + gz2) * 0.5f * da;
}
```

This loop implements the **trapezoidal rule**: the area under a curve is approximated by summing 100 thin trapezoids, each of width `da` radians. The `if` guards clamp negative GZ values to zero — a ship cannot have a negative restoring moment that contributes to stability area.

!!! tip "Loop variable scope"
    Declaring `int i` inside the `for` initialiser (`for (int i = 0; ...)`) is valid C99 and restricts `i` to the loop body. This is the preferred style in CargoForge-C — it prevents accidentally reusing a stale loop counter.

### `while` and `continue` / `break`

CargoForge-C mostly uses `for` loops with explicit bounds. The `continue` keyword skips the rest of the current iteration; `break` exits the loop entirely. You will see `continue` used as an early-exit guard:

```c
/* from perform_analysis, analysis.c:104 */
for (int i = 0; i < ship->cargo_count; i++) {
    const Cargo *c = &ship->cargo[i];
    if (c->pos_x < 0) continue;   /* skip unplaced cargo */
    /* ... accumulate moments ... */
}
```

The sentinel `pos_x < 0` marks cargo that has not been placed in a hold yet. `continue` cleanly skips those items without nesting the whole body inside an `if`.

---

## Declaring and calling functions

### Anatomy of a function

```
return_type  function_name ( parameter_list ) {
    body
    return expression;
}
```

- **Return type**: what value the function hands back to its caller. `void` means "nothing".
- **Parameters**: local copies of the data passed in (more on this below).
- **`return`**: exits the function immediately and delivers the value.

### A real example: `gz_at_angle`

The wall-sided formula for the righting lever at angle $\theta$ is:

$$GZ(\theta) = \sin\theta \left(GM + \frac{BM \cdot \tan^2\theta}{2}\right)$$

Here is the complete implementation from `analysis.c`:

```c
static float gz_at_angle(float gm, float bm, float theta_deg) {
    float theta = theta_deg * (float)M_PI / 180.0f;
    float tan_theta = tanf(theta);
    return sinf(theta) * (gm + bm * tan_theta * tan_theta / 2.0f);
}
```

Walk through it:

1. **`static`** — the function is private to `analysis.c`. Nothing outside this file can call it. This is good hygiene: it keeps the internal physics helpers from leaking into the public API.
2. **`float gz_at_angle(float gm, float bm, float theta_deg)`** — the function takes three floats and returns a float.
3. **Degrees to radians**: `<math.h>` trigonometric functions work in radians, so the first step converts $\theta$.
4. **`tanf`**, **`sinf`**: single-precision versions of $\tan$ and $\sin$. Using `tan` (double-precision) would silently promote everything to `double` and back.
5. **`return`**: the computed value is returned to whoever called `gz_at_angle`.

Calling it:

```c
/* from perform_analysis, analysis.c:214 */
r.gz_at_30 = gz_at_angle(gm_effective, r.bm, 30.0f);
```

The caller passes two local variables (`gm_effective`, `r.bm`) and a literal (`30.0f`). The function receives them as its own local copies named `gm`, `bm`, and `theta_deg`.

### `void` functions: `find_gz_max`

Some functions do not return a value — they communicate results by writing through pointer parameters. `find_gz_max` in `analysis.c` scans 1–80° in 1° steps to find where GZ peaks:

```c
static void find_gz_max(float gm, float bm, float *max_gz, float *max_angle) {
    *max_gz = 0.0f;
    *max_angle = 0.0f;
    for (float a = 1.0f; a <= 80.0f; a += 1.0f) {
        float gz = gz_at_angle(gm, bm, a);
        if (gz > *max_gz) {
            *max_gz = gz;
            *max_angle = a;
        }
    }
}
```

The `*` in `float *max_gz` means "pointer to float". Writing `*max_gz = value` stores the value at the memory address the caller provided. This is the standard C idiom for returning more than one result from a function.

Called from `perform_analysis`:

```c
find_gz_max(gm_effective, r.bm, &r.gz_max, &r.gz_max_angle);
```

The `&` operator takes the address of `r.gz_max` — it hands `find_gz_max` the location in memory where the result should be written. Pointers are covered in detail in a later lesson; for now, read `float *` as "out-parameter" and `&variable` as "the address of this variable."

---

## Pass by value

C passes arguments **by value**: the function receives a copy, not the original.

```c
float gm = 1.5f;
float result = gz_at_angle(gm, bm, 30.0f);
/* gm is still 1.5f here — gz_at_angle cannot change it */
```

Inside `gz_at_angle`, the parameter named `gm` is a separate local variable that starts with the value `1.5f`. The function can modify its own `gm` without affecting the caller's `gm`. This isolation makes functions easy to reason about — no side-effects through the parameter list unless a pointer is explicitly passed.

!!! warning "Pass by value with structs"
    Passing a large struct by value copies every byte of it. `perform_analysis` takes a `const Ship *ship` — a pointer to the Ship — rather than a `Ship ship` copy, because copying a `Ship` (with its `cargo` array pointer and all other fields) would be wasteful and semantically wrong. You will see `const Type *` used throughout when "I need to read this, not copy it."

---

## The `static` keyword on functions

`gz_at_angle`, `integrate_gz`, and `find_gz_max` are all declared `static`. This does not affect their behaviour — it restricts their **visibility** to the current translation unit (the `.c` file). The three functions are implementation details of `analysis.c`; the only public interface is `perform_analysis` and `print_loading_plan` (declared in `cargoforge.h`). Using `static` on helpers prevents name collisions across files and communicates clearly that the function is internal.

---

## Recap

- C's primitive numeric types are `int`, `float`, and `double`. CargoForge-C uses `float` for all physics quantities — adequate precision, half the memory of `double`.
- Literal floating-point constants need an `f` suffix (`1.025f`) to stay `float`; without it they are `double`.
- `for` loops follow the pattern `for (init; condition; update)`. Declaring the loop variable inside the initialiser (`int i`) keeps it scoped to the loop body.
- `continue` skips the rest of the current iteration; `break` exits the loop. Both are used as early-exit guards in the cargo accumulation loops.
- A function is declared with its return type, name, and parameter list. `static` restricts it to the current file.
- Arguments are passed **by value** — functions receive copies. To write back to the caller, pass a pointer (covered in a later lesson).
- `gz_at_angle` in `analysis.c` is a clean example: pure function, no side-effects, three floats in, one float out, directly encoding the wall-sided stability formula.

*Next: [CLI, git, and reproducible builds](04-cli-git-reproducibility.md).*
