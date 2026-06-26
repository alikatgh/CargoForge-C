# Hydrostatic interpolation

A ship's hull is not a perfect box, and a real stability calculation cannot pretend otherwise.
CargoForge-C can read a *hydrostatic table* — a CSV file supplied by the naval architect — and
interpolate between its rows to obtain draft, KB, BM, TPC, and every other property at any
displacement. This lesson traces that entire pipeline: parsing, validation, forward lookup
(draft → properties), and reverse lookup (displacement → draft).

<svg viewBox="0 0 560 224" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:540px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Hydrostatic interpolation: read a property between two table rows</title>
<desc>The hydrostatic table gives a property such as KB at discrete displacements. For a query displacement that falls between two rows, CargoForge-C interpolates linearly along the straight line joining them to obtain the value.</desc>
<line x1="72" y1="36" x2="72" y2="172" stroke="currentColor" stroke-width="1" opacity="0.5"/>
<line x1="72" y1="172" x2="500" y2="172" stroke="currentColor" stroke-width="1" opacity="0.5"/>
<text x="64" y="40" fill="currentColor" font-size="10.5" text-anchor="end" opacity="0.65">KB</text>
<text x="498" y="190" fill="currentColor" font-size="10.5" text-anchor="end" opacity="0.65">displacement →</text>
<line x1="150" y1="140" x2="410" y2="70" stroke="currentColor" stroke-width="1.2" opacity="0.45" stroke-dasharray="2 2"/>
<circle cx="150" cy="140" r="4" fill="currentColor"/><text x="150" y="158" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">row i</text>
<circle cx="410" cy="70" r="4" fill="currentColor"/><text x="410" y="60" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">row i+1</text>
<line x1="290" y1="172" x2="290" y2="105" stroke="#12A594" stroke-width="1.3" stroke-dasharray="3 2"/>
<line x1="290" y1="105" x2="72" y2="105" stroke="#12A594" stroke-width="1.3" stroke-dasharray="3 2"/>
<circle cx="290" cy="105" r="4.5" fill="#12A594"/>
<text x="290" y="188" fill="#12A594" font-size="10" text-anchor="middle">query Δ</text>
<text x="80" y="100" fill="#12A594" font-size="10">interpolated KB</text>
<text x="280" y="214" fill="currentColor" font-size="11" text-anchor="middle" opacity="0.65">Linear interpolation between the two nearest rows (<tspan font-family="var(--md-code-font,monospace)">src/hydrostatics.c</tspan>); box model if no table.</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Hydrostatic table** = "a lookup chart that maps how deep the ship sits to every hull property at that depth" — instead of guessing with a generic `BLOCK_COEFF`, `perform_analysis` reads a CSV supplied by the naval architect and uses real numbers for your specific hull.
- **`HydroEntry`** = "one row of that chart — draft, displacement, KB, BM, KM, TPC, MTC, and optionally waterplane area and LCB all in one struct" — `parse_hydro_table` fills an array of these from the CSV; the rest of the code reads from that array rather than computing approximations.
- **Linear interpolation / `lerp`** = "splitting the gap between two known values in proportion to where your query lands" — when the ship floats at 7.389 m and the table only has rows at 7.0 m and 8.0 m, `lerp` slides 38.9 % of the way along every field simultaneously via `interpolate_entries`.
- **Forward lookup (`hydro_interpolate`)** = "given a draft in metres, return all the hull properties at that depth" — `perform_analysis` calls this after it already knows the draft, to fill in KB, BM, KM, and the rest in one shot.
- **Reverse lookup (`hydro_draft_from_displacement`)** = "given total weight in tonnes, figure out how deep the ship sits" — this runs first in `perform_analysis` because you know the cargo mass before you know the draft; then the result feeds straight into `hydro_interpolate`.
- **Clamping** = "if your query is outside the table's range, return the nearest endpoint instead of guessing beyond it" — both lookup functions do this at their top and bottom boundaries so an overloaded ship gets the deepest table row rather than a made-up extrapolation.
- **Ascending-draft validation** = "the parser refuses a table whose rows are out of order" — a hard error at parse time rather than a warning, because the bracketing loop in `hydro_interpolate` only works correctly when rows are sorted; a disorder would silently produce wrong KB and GM values.

**Why it matters:** if the table is wrong, missing rows, or out-of-order, every downstream stability number — GM, trim, free-surface correction — is built on bad data, and a ship that looks stable on paper may not be; the parser's strict validation and the clamping guards exist precisely to catch those silent failure modes before they reach the final stability report.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> Imagine a long ruler lying on a table. You've marked two points on it: "7.0 m draft, KB = 3.71 m" at the left, and "8.0 m draft, KB = 4.24 m" at the right. Your ship floats at 7.389 m — so you slide your finger 38.9 % of the way along the ruler and read off the KB right there. That's the whole idea.

`lerp` is the finger-sliding. `interpolate_entries` does it for all nine `HydroEntry` fields — `kb`, `bm`, `km`, `tpc`, `mtc`, and the rest — simultaneously, using the same fraction `t`. The fraction comes from whichever column you're indexing: draft for `hydro_interpolate`, displacement for `hydro_draft_from_displacement`.

The two-step pipeline in `perform_analysis` is just doing this twice in sequence: first slide along the *displacement* column to find draft, then slide along the *draft* column to read KB, BM, and KM — which feed directly into GM = KB + BM − KG.

## Why tables instead of formulas?

The box-hull fallback bakes in constants like `BLOCK_COEFF = 0.75` and `KB_FACTOR = 0.53`.
Those are reasonable approximations for a generic cargo ship; they are wrong for your specific
hull. A real vessel comes with hydrostatic curves computed from its actual lines plan. When you
supply that data as a CSV, `perform_analysis` sets `result.hydro_table_used = 1` and every
stability number — draft, KB, BM, KM, trim — is derived from the table instead of the
approximation.

---

## The data structure

Each row of the CSV becomes a `HydroEntry` (from `hydrostatics.h`):

| Field | Meaning |
|---|---|
| `draft` | Water depth under keel (m) |
| `displacement` | Mass of displaced water (t) |
| `km` | Distance from keel to metacentre (m) |
| `kb` | Distance from keel to centre of buoyancy (m) |
| `bm` | Transverse metacentric radius (m) |
| `tpc` | Tonnes per centimetre immersion |
| `mtc` | Moment to change trim 1 cm (t·m/cm) |
| `waterplane_area` | Area of the waterplane (m²) — optional, 8th column |
| `lcb` | Longitudinal centre of buoyancy from midship (m) — optional, 9th column |

The table is stored as an array of these entries inside a `HydroTable`, which also holds
`count` (number of valid rows) and a `loaded` flag that `perform_analysis` checks.

---

## Parsing the CSV — `parse_hydro_table`

The function lives in [`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c). It opens the file, skips comment lines (those
starting with `#`) and blank lines, then calls `sscanf` on each remaining line:

```c
/* from src/hydrostatics.c */
int parsed = sscanf(line, "%f,%f,%f,%f,%f,%f,%f,%f,%f",
                    &e->draft, &e->displacement, &e->km, &e->kb,
                    &e->bm, &e->tpc, &e->mtc, &e->waterplane_area,
                    &e->lcb);

if (parsed < 7) {
    fprintf(stderr, "Warning: Skipping malformed hydrostatic entry at line %d "
            "(got %d fields, need at least 7)\n", line_num, parsed);
    continue;
}

/* Default LCB to 0 if not provided (8-column format) */
if (parsed < 9) e->lcb = 0.0f;
/* Default waterplane_area to 0 if not provided (7-column format) */
if (parsed < 8) e->waterplane_area = 0.0f;
```

The format is tolerant: 7, 8, or 9 columns are all accepted. A malformed row is skipped
with a warning rather than aborting the whole file. This is intentional — a hydrostatic table
from a third-party tool may include header lines or comment rows that do not conform exactly.

### Ascending-draft validation

After each row is accepted, the parser checks that it continues the sequence in strictly
increasing draft order:

```c
/* from src/hydrostatics.c */
if (table->count > 0 &&
    e->draft <= table->entries[table->count - 1].draft) {
    fprintf(stderr, "Error: Hydrostatic table not in ascending draft order "
            "at line %d (%.2f <= %.2f)\n",
            line_num, e->draft, table->entries[table->count - 1].draft);
    fclose(fp);
    return -1;
}
```

This is a hard error, not a warning, because the interpolation algorithm depends on the rows
being sorted. An out-of-order table would silently produce garbage results if the parser let
it through.

Finally, after the loop, the function refuses a table with fewer than two rows, because you
cannot interpolate between a single point:

```c
if (table->count < 2) {
    fprintf(stderr, "Error: Hydrostatic table needs at least 2 entries (got %d)\n",
            table->count);
    return -1;
}
```

A valid parse sets `table->loaded = 1` and returns 0.

---

## Linear interpolation — the core idea

Once the table is in memory, CargoForge-C needs to answer queries like "what is KB when the
draft is 7.43 m?" The table probably has entries at 7.0 m and 8.0 m but not at 7.43 m exactly.

*Linear interpolation* assumes the value changes at a constant rate between the two nearest
table rows. If $f_0$ is the value at draft $d_0$ and $f_1$ is the value at draft $d_1$, and
you want the value at draft $d$ where $d_0 \le d \le d_1$:

$$t = \frac{d - d_0}{d_1 - d_0}, \qquad f(d) = f_0 + t \cdot (f_1 - f_0)$$

$t$ is the *interpolation fraction*, a number in $[0, 1]$. When $t = 0$ you are at the lower
row; when $t = 1$ you are at the upper row; when $t = 0.5$ you are exactly halfway.

In [`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c), this is a two-function stack:

```c
/* from src/hydrostatics.c */
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static void interpolate_entries(const HydroEntry *lo, const HydroEntry *hi,
                                float t, HydroEntry *out) {
    out->draft           = lerp(lo->draft,           hi->draft,           t);
    out->displacement    = lerp(lo->displacement,    hi->displacement,    t);
    out->km              = lerp(lo->km,              hi->km,              t);
    out->kb              = lerp(lo->kb,              hi->kb,              t);
    out->bm              = lerp(lo->bm,              hi->bm,              t);
    out->tpc             = lerp(lo->tpc,             hi->tpc,             t);
    out->mtc             = lerp(lo->mtc,             hi->mtc,             t);
    out->waterplane_area = lerp(lo->waterplane_area, hi->waterplane_area, t);
    out->lcb             = lerp(lo->lcb,             hi->lcb,             t);
}
```

`lerp` handles one scalar. `interpolate_entries` applies the same fraction $t$ to every
field simultaneously, filling the output `HydroEntry` with the interpolated row.

!!! note "Why the same $t$ for every field?"
    Because all hydrostatic properties are indexed against the same independent variable:
    draft. One fraction $t$ correctly positions you between rows for all of them at once.

---

## Forward lookup: draft → properties (`hydro_interpolate`)

The forward query answers "given a draft, give me all properties." This is what
`perform_analysis` calls after it has found the draft from the displacement:

```c
/* from src/hydrostatics.c */
int hydro_interpolate(const HydroTable *table, float draft, HydroEntry *result) {
    if (!table || !result || table->count == 0)
        return -1;

    /* Clamp to table boundaries */
    if (draft <= table->entries[0].draft) {
        *result = table->entries[0];
        return 0;
    }
    if (draft >= table->entries[table->count - 1].draft) {
        *result = table->entries[table->count - 1];
        return 0;
    }

    /* Find bounding entries */
    for (int i = 0; i < table->count - 1; i++) {
        const HydroEntry *lo = &table->entries[i];
        const HydroEntry *hi = &table->entries[i + 1];

        if (draft >= lo->draft && draft <= hi->draft) {
            float range = hi->draft - lo->draft;
            float t = (range > 1e-6f) ? (draft - lo->draft) / range : 0.0f;
            interpolate_entries(lo, hi, t, result);
            return 0;
        }
    }

    *result = table->entries[table->count - 1];
    return 0;
}
```

Three cases in order:

1. **Below the table minimum**: return the first row exactly. This is *clamping* — the ship
   cannot be lighter than its lightest ballast condition, so the table's bottom row is the
   best available answer.
2. **Above the table maximum**: return the last row. The ship should never be loaded beyond
   the table's deepest draft; clamping here prevents extrapolation into unknown territory.
3. **Within the table**: walk the rows until finding the bracket $[lo, hi]$ that contains the
   query draft, compute $t$, call `interpolate_entries`.

The guard `range > 1e-6f` prevents a divide-by-zero if two consecutive rows somehow have
identical draft values — that should not happen after the ascending-order check during
parsing, but defensive programming is cheap.

---

## Reverse lookup: displacement → draft (`hydro_draft_from_displacement`)

`perform_analysis` knows the ship's total displacement in tonnes (lightship + cargo + ballast
tanks). It needs the draft that corresponds to that displacement — the inverse of the normal
direction. The same interpolation idea applies, but now displacement is the independent
variable:

```c
/* from src/hydrostatics.c */
float hydro_draft_from_displacement(const HydroTable *table, float displacement_t) {
    if (!table || table->count == 0)
        return -1.0f;

    /* Clamp to table boundaries */
    if (displacement_t <= table->entries[0].displacement)
        return table->entries[0].draft;
    if (displacement_t >= table->entries[table->count - 1].displacement)
        return table->entries[table->count - 1].draft;

    /* Find bounding entries by displacement and inverse-interpolate */
    for (int i = 0; i < table->count - 1; i++) {
        float disp_lo = table->entries[i].displacement;
        float disp_hi = table->entries[i + 1].displacement;

        if (displacement_t >= disp_lo && displacement_t <= disp_hi) {
            float range = disp_hi - disp_lo;
            float t = (range > 1e-6f) ? (displacement_t - disp_lo) / range : 0.0f;
            return lerp(table->entries[i].draft, table->entries[i + 1].draft, t);
        }
    }

    return table->entries[table->count - 1].draft;
}
```

The structure is identical to `hydro_interpolate` — clamp, walk, bracket, compute $t$ — but
the fraction is computed from the *displacement* span, and only the *draft* is returned
(a `float`, not a full `HydroEntry`). After this call, `perform_analysis` passes the returned
draft back into `hydro_interpolate` to fill in KB, BM, and the rest.

!!! tip "The two-step pipeline in `perform_analysis`"
    The analysis code in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) uses both functions in sequence:

    1. Compute total displacement (lightship + cargo + tanks).
    2. Call `hydro_draft_from_displacement` → get draft (m).
    3. Call `hydro_interpolate` at that draft → get KB, BM, KM, MTC, etc.

    This cleanly separates "what draft do we float at?" from "what are the hull properties at
    that draft?"

---

## A worked example

Suppose the table has two rows:

| draft (m) | displacement (t) | KB (m) | BM (m) |
|---|---|---|---|
| 7.0 | 12 400 | 3.71 | 4.88 |
| 8.0 | 14 200 | 4.24 | 4.61 |

The ship's displacement is 13 100 t. What is the draft?

$$t = \frac{13\,100 - 12\,400}{14\,200 - 12\,400} = \frac{700}{1800} \approx 0.389$$

$$\text{draft} = 7.0 + 0.389 \times (8.0 - 7.0) = 7.389 \text{ m}$$

Now query KB at that draft:

$$KB = 3.71 + 0.389 \times (4.24 - 3.71) = 3.71 + 0.206 = 3.916 \text{ m}$$

And $GM = KB + BM - KG$. With BM similarly interpolated and KG computed from the cargo
moments:

$$GM = KB + BM - KG$$

The free-surface correction then subtracts from GM to give $GM_{\text{corrected}}$, which
must clear the IMO minimum of 0.15 m.

!!! warning "Table coverage matters"
    If the table only covers 5–9 m draft but the loaded ship draws 9.8 m, CargoForge-C
    clamps to the 9 m row. The result is reported, but it is an extrapolation. Always supply
    a table that covers the full operational draft range of your vessel.

---

## Recap

- `parse_hydro_table` reads a comma-separated CSV (7–9 columns), skips comments, and
  enforces strictly ascending draft order — failing hard on violations because unsorted data
  would corrupt all downstream calculations.
- At least two rows are required; a single-row table cannot produce an interpolation.
- `lerp(a, b, t)` is the scalar primitive; `interpolate_entries` applies it across all nine
  `HydroEntry` fields at the same fraction $t$.
- `hydro_interpolate` answers "draft → all properties" by bracketing the query between
  adjacent rows and computing $t$ from the draft span.
- `hydro_draft_from_displacement` answers the reverse question — "displacement → draft" —
  using the identical pattern but bracketing on the displacement column.
- Both functions clamp at table boundaries rather than extrapolating; values outside the
  table range return the nearest endpoint.

*Next: [Tanks and free surface](36-tanks-and-free-surface.md).*
