# Displacement, density, and draft

Every stability number in CargoForge-C — GM, trim, heel, the IMO criteria — depends on knowing
how deep the ship sits in the water. That depth is **draft**, and it follows directly from two
physical quantities: how much the ship weighs (its **displacement**) and how dense the water is.
This lesson walks through the physics and then traces exactly how `perform_analysis` in
[`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) derives draft from first principles, with a real hydrostatic table as the
preferred path and a box-hull formula as the fallback.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Displacement** = "the total weight of everything on the ship, in tonnes" — `perform_analysis` adds `lightship_weight`, the weight of all placed cargo (items where `pos_x >= 0`), and tank contents to get this single number; everything else in the lesson flows from it.
- **Displaced volume** = "the blob of water the hull has to push aside to stay afloat" — computed as `displacement_t / SEAWATER_DENSITY` (1.025 t/m³), it converts tonnes of ship into cubic metres of water, which is the shape the hull must carve out.
- **Draft** = "how many metres of hull are underwater right now" — the lesson shows two ways CargoForge-C finds it: a quick box-hull formula (`displaced_vol / (length × width × 0.75)`) and an accurate inverse-interpolation through the ship's hydrostatic table via `hydro_draft_from_displacement`.
- **Block coefficient (C_b)** = "how boxy the underwater hull is, on a 0-to-1 scale" — hardcoded at 0.75 in the fallback path; it shrinks the bounding-box volume to account for the rounded bow and tapered stern that a real hull has.
- **Hydrostatic table** = "a pre-computed cheat-sheet of hull geometry at every possible draft" — when one is loaded, `hydro_draft_from_displacement` finds draft by bracketing the target displacement between two table rows and interpolating, then `hydro_interpolate` hands back KB, BM, and MTC at that exact draft.
- **Overweight guard** = "a hard stop before any physics runs" — if total displacement exceeds `ship->max_weight`, `perform_analysis` sets `r.gm = NAN` and returns immediately; downstream code (JSON output, CLI report) treats NaN GM as the signal that the plan is invalid.

**Why it matters:** Draft is the root input for every stability number — GM, trim, KB, BM, and longitudinal strength all depend on it. Get displacement wrong (e.g. by including unplaced cargo, or using fresh-water density instead of 1.025) and every downstream stability calculation will be off.

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> Imagine lowering a fully loaded freight elevator into a swimming pool. The pool water
> rises exactly enough to make room for the elevator's weight — not its size, its *weight*.
> The denser the water, the less it has to rise. The boxier the elevator, the shallower the rise.

That's the whole lesson. In CargoForge-C, the "elevator" is `displacement_t` (lightship + placed cargo + tanks). The "rise" is `r.draft`. The pool is denser than fresh water (`SEAWATER_DENSITY = 1.025f`), so the hull sits slightly higher than it would in a river. And the "boxiness" is `BLOCK_COEFF = 0.75` in the fallback path — or, when you load a hydrostatic CSV, the real hull geometry encoded row-by-row in `ship->hydro`. `hydro_draft_from_displacement` is just the pool asking: "given this much elevator weight, how high does the water line sit on the hull?"

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Displacement → volume → draft pipeline</title>
  <desc>Three-stage pipeline showing how CargoForge-C converts displacement_kg to displaced_vol to r.draft, with the overweight guard at the start and the two draft paths (box-hull fallback and hydrostatic table) at the end.</desc>

  <!-- Stage 1: displacement_kg box -->
  <rect x="10" y="100" width="150" height="110" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="85" y="126" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">displacement_kg</text>
  <text x="85" y="144" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">lightship_weight</text>
  <text x="85" y="159" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">+ cargo (pos_x ≥ 0)</text>
  <text x="85" y="174" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">+ tank_weight × 1000</text>
  <!-- overweight guard label -->
  <rect x="18" y="193" width="134" height="10" rx="3" fill="#D05663" opacity="0.15"/>
  <text x="85" y="201" text-anchor="middle" font-size="9" fill="#D05663" font-weight="600">⚠ if > max_weight → NAN</text>

  <!-- Arrow 1: ÷ 1000 -->
  <line x1="160" y1="155" x2="198" y2="155" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <polygon points="198,151 206,155 198,159" fill="currentColor" opacity="0.6"/>
  <text x="183" y="148" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">÷ 1000</text>
  <text x="183" y="170" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.55">→ tonnes</text>

  <!-- Stage 2: displacement_t box -->
  <rect x="206" y="115" width="120" height="80" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="266" y="143" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">displacement_t</text>
  <text x="266" y="161" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">(tonnes)</text>
  <text x="266" y="181" text-anchor="middle" font-size="10" fill="#12A594">÷ 1.025 t/m³</text>

  <!-- Arrow 2: ÷ ρ -->
  <line x1="326" y1="155" x2="364" y2="155" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <polygon points="364,151 372,155 364,159" fill="currentColor" opacity="0.6"/>
  <text x="348" y="148" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">÷ ρ</text>
  <text x="348" y="170" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.55">→ m³</text>

  <!-- Stage 3: displaced_vol box -->
  <rect x="372" y="115" width="120" height="80" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="432" y="143" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">displaced_vol</text>
  <text x="432" y="161" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.75">(m³)</text>
  <text x="432" y="181" text-anchor="middle" font-size="10" fill="#12A594">hull geometry →</text>

  <!-- Fork arrow down-right to two paths -->
  <line x1="492" y1="155" x2="530" y2="155" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <line x1="530" y1="155" x2="530" y2="90" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <line x1="530" y1="155" x2="530" y2="220" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <polygon points="526,90 530,82 534,90" fill="currentColor" opacity="0.5"/>
  <polygon points="526,220 530,228 534,220" fill="currentColor" opacity="0.5"/>

  <!-- Path A: box-hull fallback -->
  <rect x="540" y="58" width="72" height="48" rx="5"
        fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.6"/>
  <text x="576" y="78" text-anchor="middle" font-size="9.5" font-weight="600" fill="currentColor">Box-hull</text>
  <text x="576" y="91" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">V / (L×B×Cb)</text>
  <text x="576" y="102" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">Cb = 0.75</text>

  <!-- Path B: hydrostatic table -->
  <rect x="540" y="194" width="72" height="48" rx="5"
        fill="none" stroke="#12A594" stroke-width="1.5" opacity="0.85"/>
  <text x="576" y="213" text-anchor="middle" font-size="9.5" font-weight="600" fill="#12A594">Hydro table</text>
  <text x="576" y="226" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">hydro_draft_from</text>
  <text x="576" y="237" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.7">_displacement()</text>

  <!-- r.draft label spanning both -->
  <text x="576" y="280" text-anchor="middle" font-size="11" font-weight="700" fill="#12A594">r.draft (m)</text>
  <line x1="576" y1="106" x2="576" y2="272" stroke="#12A594" stroke-width="1" stroke-dasharray="3,3" opacity="0.4"/>
  <line x1="576" y1="242" x2="576" y2="272" stroke="#12A594" stroke-width="1" stroke-dasharray="3,3" opacity="0.4"/>

  <!-- Stage labels at top -->
  <text x="85" y="88" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.5">① accumulate</text>
  <text x="266" y="105" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.5">② convert</text>
  <text x="432" y="105" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.5">③ volume</text>
  <text x="576" y="48" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.5">④ draft</text>
</svg>

## What displacement means

The word "displacement" comes from Archimedes: a floating body displaces a volume of water
equal in *weight* to the body itself. So a ship that weighs 15 000 tonnes displaces exactly
15 000 tonnes of water, regardless of its shape.

In CargoForge-C the displacement in kilograms is assembled in `perform_analysis`
([`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c)):

```c
float displacement_kg = ship->lightship_weight + r.total_cargo_weight_kg;

/* Include tank weight in displacement if tanks are loaded */
float tank_weight_t = 0.0f;
if (ship->tanks && ship->tanks->count > 0) {
    tank_weight_t = calculate_tank_weight(ship->tanks);
    displacement_kg += tank_weight_t * 1000.0f;
}
```

Three contributors, always:

| Term | Source |
|---|---|
| `ship->lightship_weight` | Ship config key `lightship_weight_tonnes` × 1000 (kg) |
| `r.total_cargo_weight_kg` | Sum of placed cargo items only (`pos_x >= 0`) |
| `tank_weight_t * 1000` | Sum of (length × breadth × height × fill × density) for each tank |

The guard `if (c->pos_x < 0) continue;` in the cargo accumulation loop means unplaced items do
not contribute to displacement. A container sitting on the dock weighs nothing for purposes of
how deep the ship floats.

!!! note "Tonnes versus kilograms"
    The code stores all weights internally in **kilograms** but the physics of buoyancy works
    more naturally in **tonnes** (1 t = 1000 kg). You will see `displacement_t =
    displacement_kg / 1000.0f` immediately after the accumulation. Both variables live together
    for the rest of `perform_analysis`; always check the suffix (`_kg` vs `_t`) before using one
    in a formula.

---

## From mass to volume: the density step

A tonne of seawater and a tonne of fresh water occupy different volumes. CargoForge-C uses the
standard oceanographic value at 15 °C:

```c
#define SEAWATER_DENSITY    1.025f  /* t/m3 at 15C */
```

The **displaced volume** is:

$$V = \frac{\Delta}{\rho}$$

where $\Delta$ is displacement in tonnes and $\rho$ is seawater density in t/m³. In code
([`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c)):

```c
float displacement_t = displacement_kg / 1000.0f;
float displaced_vol  = displacement_t / SEAWATER_DENSITY;
```

If the ship displaces 10 000 t, it displaces $10\,000 / 1.025 \approx 9756\,\text{m}^3$ of
water. This volume is the geometric quantity that determines how deep the hull must sit.

!!! tip "Why 1.025 and not 1.000?"
    Pure fresh water is 1.000 t/m³. Seawater carries dissolved salts — roughly 35 grams per
    litre at open-ocean salinity — raising its density to about 1.025 t/m³. A ship loaded to
    its freshwater marks will sink slightly deeper than at its saltwater marks. CargoForge-C
    uses the seawater value throughout.

---

## Draft: how deep the hull sits

Given the displaced volume, draft $T$ can be found if you know the shape of the underwater
hull. CargoForge-C supports two modes.

### Box-hull fallback

When no hydrostatic CSV is loaded (`ship->hydro` is NULL), the code treats the hull as a
rectangular box scaled by two empirical coefficients:

$$T = \frac{V}{L \times B \times C_b}$$

```c
#define BLOCK_COEFF  0.75f   /* Cb: underwater hull vol / bounding box */

/* ---- Legacy box-hull fallback ---- */
r.hydro_table_used = 0;
r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
```

The **block coefficient** $C_b = 0.75$ accounts for the fact that a real hull is not a
perfect rectangular prism — the bow tapers, the bilge rounds, the stern narrows. A typical
cargo ship has $C_b$ between 0.65 and 0.85; 0.75 is a reasonable midpoint for a first
approximation.

| Quantity | Symbol | Value in code |
|---|---|---|
| Displaced volume | $V$ | `displaced_vol` (m³) |
| Ship length | $L$ | `ship->length` (m) |
| Ship beam | $B$ | `ship->width` (m) |
| Block coefficient | $C_b$ | `BLOCK_COEFF = 0.75` |

!!! warning "Approximation, not precision"
    The box-hull formula ignores hull form entirely. It can over- or under-estimate draft by
    10–20 % on a real ship. Use it only when no hydrostatic table is available, and always
    prefer the table path in production.

### Table-based draft (preferred path)

A hydrostatic table is a pre-computed lookup: at each possible draft, a naval architect has
calculated displacement (and KB, BM, MTC, and so on) from the actual hull geometry. Loading
one with the ship config key `hydrostatic_table` switches `perform_analysis` to the accurate
path:

```c
if (ship->hydro && ship->hydro->loaded) {
    r.hydro_table_used = 1;
    r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);

    HydroEntry he;
    hydro_interpolate(ship->hydro, r.draft, &he);

    r.kb = he.kb;
    r.bm = he.bm;
    /* ... */
}
```

Two functions in [`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c) do the work.

#### `hydro_draft_from_displacement` — inverse interpolation

The table is indexed by draft, but here we know displacement and need draft. The function
searches the table for the two rows that bracket the target displacement, then interpolates:

```c
float hydro_draft_from_displacement(const HydroTable *table, float displacement_t) {
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

The interpolation fraction $t$ is:

$$t = \frac{\Delta - \Delta_{\text{lo}}}{\Delta_{\text{hi}} - \Delta_{\text{lo}}}$$

and draft is then $T = T_{\text{lo}} + t \times (T_{\text{hi}} - T_{\text{lo}})$. The guard
`range > 1e-6f` avoids a divide-by-zero if two consecutive rows happen to have identical
displacement.

#### `hydro_interpolate` — forward interpolation at that draft

Once draft is known, `hydro_interpolate` returns KB, BM, MTC, and the other properties for
that exact draft by the same linear approach:

```c
float range = hi->draft - lo->draft;
float t = (range > 1e-6f) ? (draft - lo->draft) / range : 0.0f;
interpolate_entries(lo, hi, t, result);
```

`interpolate_entries` applies `lerp` component-by-component to every field of `HydroEntry`.

#### The CSV format

A hydrostatic table CSV requires at least 7 columns per row, in ascending draft order:

```
# draft_m, displacement_t, km_m, kb_m, bm_m, tpc_t_per_cm, mtc_tm_per_cm
3.0,  4200, 5.80, 1.59, 4.21, 8.4,  55.2
4.0,  5800, 5.72, 2.12, 3.60, 8.7,  58.6
5.0,  7500, 5.68, 2.65, 3.03, 9.1,  62.1
```

`parse_hydro_table` ([`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c)) rejects rows with fewer than 7 fields and enforces
the ascending-draft invariant:

```c
if (table->count > 0 &&
    e->draft <= table->entries[table->count - 1].draft) {
    fprintf(stderr, "Error: Hydrostatic table not in ascending draft order ...\n");
    return -1;
}
```

A minimum of 2 rows is required — you cannot interpolate with only one point.

---

## Overweight rejection

Before any draft or stability calculation is attempted, `perform_analysis` checks whether the
total displacement exceeds the ship's structural limit:

```c
if (displacement_kg > ship->max_weight) {
    r.gm = NAN;
    return r;
}
```

Returning `NAN` in `r.gm` is the signal used throughout the codebase (including JSON output
and the CLI report) that the loading plan is invalid. No further physics is computed.

---

## How the pieces connect

The chain from loading plan to draft is short and clean:

$$\underbrace{\text{cargo weights + lightship + tanks}}_{\text{displacement}_{\,\Delta\,\text{(kg)}}}
\xrightarrow{\div 1000}
\Delta\,\text{(t)}
\xrightarrow{\div \rho}
V\,\text{(m}^3\text{)}
\xrightarrow{\text{hull geometry}}
T\,\text{(m)}$$

Draft $T$ then feeds into every subsequent calculation: KB depends on $T$, BM depends on the
displaced volume, trim uses the hydrostatic MTC at $T$, and longitudinal strength integrates
over the load at each of 20 hull stations spaced along the full length.

!!! example "Concrete numbers"
    A ship with $L = 150\,\text{m}$, $B = 22\,\text{m}$, lightship 8 000 t, carrying
    4 500 t of placed cargo and 200 t in tanks:

    $$\Delta = 8000 + 4500 + 200 = 12\,700\,\text{t}$$

    $$V = \frac{12700}{1.025} \approx 12\,390\,\text{m}^3$$

    Box-hull draft: $T = \frac{12390}{150 \times 22 \times 0.75} \approx 5.02\,\text{m}$

    If a hydrostatic table bracketed this displacement between 12 500 t at 5.05 m and
    13 000 t at 5.30 m, the table path would give:
    $t = (12700 - 12500)/(13000 - 12500) = 0.40$,
    $T = 5.05 + 0.40 \times 0.25 = 5.15\,\text{m}$ — a 13 cm difference, which matters for
    GM and for checking freeboard limits.

---

## Recap

- **Displacement** is the total weight of ship + cargo + tanks, measured in tonnes. A floating
  body displaces its own weight in water.
- **Displaced volume** = displacement (t) / seawater density (1.025 t/m³). This is the
  geometric quantity that determines how deep the hull sits.
- **Draft (box-hull fallback)** = displaced volume / (L × B × $C_b$), with $C_b = 0.75$.
  Fast but approximate.
- **Draft (table path)** uses `hydro_draft_from_displacement` to inverse-interpolate the
  ship's pre-computed hydrostatic table — accurate because it reflects the real hull form.
- Once draft is known, `hydro_interpolate` returns KB, BM, MTC and the rest, which feed
  every downstream stability calculation.
- An overweight loading plan is caught before any physics: `r.gm = NAN` signals rejection.

*Next: [The box-hull model](19-the-box-hull-model.md).*
