# Displacement, density, and draft

Every stability number in CargoForge-C — GM, trim, heel, the IMO criteria — depends on knowing
how deep the ship sits in the water. That depth is **draft**, and it follows directly from two
physical quantities: how much the ship weighs (its **displacement**) and how dense the water is.
This lesson walks through the physics and then traces exactly how `perform_analysis` in
[`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) derives draft from first principles, with a real hydrostatic table as the
preferred path and a box-hull formula as the fallback.

## The mental model 🧠

**Displacement** is the ship's weight told as a volume of water. A loaded ship floats at exactly the depth where it has pushed aside its own weight in water — so "how much does it weigh?" and "how much water has it shoved out of the way?" are the same question in different units. The depth it settles to is the **draft**.

Two things move the draft. Add cargo and the ship must displace more water, so it sinks deeper — draft rises. Sail from salt sea into a fresh-water river and the *water* gets less dense, so the same weight needs *more* volume to balance, and the ship sinks deeper there too — which is why load lines are marked separately for salt and fresh water. CargoForge runs the calculation in the direction operators actually need: it knows the total weight and works *backwards* to the draft — preferring a measured hydrostatic table, and falling back to the box-hull formula when none is loaded.

<svg viewBox="0 0 600 240" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Displacement and draft: weight down balances the buoyancy of displaced water up</title>
<desc>The ship floats at the draft where its weight (its displacement) is exactly balanced by the upward buoyancy of the water its hull pushes aside. Adding cargo increases displacement, so the ship sinks deeper and the draft increases.</desc>
<rect x="0" y="96" width="600" height="120" fill="#12A594" fill-opacity="0.06"/>
<line x1="0" y1="96" x2="600" y2="96" stroke="#12A594" stroke-opacity="0.5"/>
<text x="20" y="90" font-size="9.5" fill="#12A594" opacity="0.8">water surface</text>
<path d="M180,60 L420,60 L408,182 L192,182 Z" fill="currentColor" fill-opacity="0.06" stroke="currentColor" stroke-opacity="0.6" stroke-width="1.2"/>
<text x="300" y="50" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">hull</text>
<line x1="284" y1="70" x2="284" y2="146" stroke="#D05663" stroke-width="1.6"/><path d="M279,139 L284,148 L289,139" fill="#D05663"/>
<text x="250" y="86" font-size="10" text-anchor="end" fill="#D05663">W ↓</text>
<text x="250" y="100" font-size="8.5" text-anchor="end" fill="currentColor" opacity="0.6">weight</text>
<line x1="316" y1="176" x2="316" y2="100" stroke="#12A594" stroke-width="1.6"/><path d="M311,107 L316,98 L321,107" fill="#12A594"/>
<text x="330" y="170" font-size="10" fill="#12A594">ρgV ↑</text>
<text x="330" y="184" font-size="8.5" fill="currentColor" opacity="0.6">buoyancy</text>
<line x1="452" y1="96" x2="452" y2="182" stroke="currentColor" stroke-opacity="0.6"/><path d="M448,103 L452,95 L456,103" fill="none" stroke="currentColor" stroke-opacity="0.6"/><path d="M448,175 L452,183 L456,175" fill="none" stroke="currentColor" stroke-opacity="0.6"/>
<text x="462" y="143" font-size="10.5" fill="currentColor" opacity="0.8">draft T</text>
<line x1="192" y1="120" x2="414" y2="120" stroke="#D05663" stroke-opacity="0.5" stroke-dasharray="4 3"/>
<text x="300" y="206" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">add cargo → more displacement → sinks deeper (draft ↑)</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Displacement** = "the total weight of everything on the ship, in tonnes" — `perform_analysis` adds `lightship_weight`, the weight of all placed cargo (items where `pos_x >= 0`), and tank contents to get this single number; everything else in the lesson flows from it.
- **Displaced volume** = "the blob of water the hull has to push aside to stay afloat" — computed as `displacement_t / SEAWATER_DENSITY` (1.025 t/m³), it converts tonnes of ship into cubic metres of water, which is the shape the hull must carve out.
- **Draft** = "how many metres of hull are underwater right now" — the lesson shows two ways CargoForge-C finds it: a quick box-hull formula (`displaced_vol / (length × width × 0.75)`) and an accurate inverse-interpolation through the ship's hydrostatic table via `hydro_draft_from_displacement`.
- **Block coefficient (C_b)** = "how boxy the underwater hull is, on a 0-to-1 scale" — hardcoded at 0.75 in the fallback path; it shrinks the bounding-box volume to account for the rounded bow and tapered stern that a real hull has.
- **Hydrostatic table** = "a pre-computed cheat-sheet of hull geometry at every possible draft" — when one is loaded, `hydro_draft_from_displacement` finds draft by bracketing the target displacement between two table rows and interpolating, then `hydro_interpolate` hands back KB, BM, and MTC at that exact draft.
- **Overweight guard** = "a hard stop before any physics runs" — if total displacement exceeds `ship->max_weight`, `perform_analysis` sets `r.gm = NAN` and returns immediately; downstream code (JSON output, CLI report) treats NaN GM as the signal that the plan is invalid.

**Why it matters:** Draft is the root input for every stability number — GM, trim, KB, BM, and longitudinal strength all depend on it. Get displacement wrong (e.g. by including unplaced cargo, or using fresh-water density instead of 1.025) and every downstream stability calculation will be off.

---

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
