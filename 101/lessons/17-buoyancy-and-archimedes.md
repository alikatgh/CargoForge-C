# Buoyancy and Archimedes

Steel is about eight times denser than water, yet a ship made of steel floats. Understanding why is not a physics curiosity — it is the prerequisite for every stability number CargoForge-C computes. `perform_analysis` in `src/analysis.c` derives draft, KB, BM, and ultimately GM from a single foundational calculation: how much water does the ship displace, and what does that tell us?

<svg viewBox="0 0 600 232" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Why a steel ship floats: buoyancy equals the weight of the displaced water</title>
<desc>The ship's submerged hull pushes aside a volume of water V. The upward buoyant force equals the weight of that displaced water. The ship floats at the draft where buoyancy up balances the ship's weight down.</desc>
<defs>
<marker id="bw-dn" viewBox="0 0 10 10" refX="5" refY="9" markerWidth="7" markerHeight="7" orient="auto"><path d="M5 10 L1 1 L9 1 Z" fill="#D05663"/></marker>
<marker id="bw-up" viewBox="0 0 10 10" refX="5" refY="1" markerWidth="7" markerHeight="7" orient="auto"><path d="M5 0 L9 9 L1 9 Z" fill="#12A594"/></marker>
</defs>
<rect x="40" y="120" width="520" height="90" fill="#12A594" fill-opacity="0.07"/>
<line x1="40" y1="120" x2="560" y2="120" stroke="#12A594" stroke-width="1.2" opacity="0.5"/>
<text x="48" y="114" fill="#12A594" font-size="10.5" opacity="0.75">waterline</text>
<path d="M190,72 L214,176 Q224,188 252,186 L348,186 Q376,188 386,176 L410,72" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-width="1.4" opacity="0.8"/>
<line x1="190" y1="72" x2="410" y2="72" stroke="currentColor" stroke-width="1.4" opacity="0.8"/>
<text x="300" y="166" fill="#12A594" font-size="10.5" text-anchor="middle" opacity="0.9">displaced water · volume V</text>
<line x1="268" y1="150" x2="268" y2="92" stroke="#D05663" stroke-width="1.8" marker-end="url(#bw-dn)"/>
<text x="268" y="86" fill="#D05663" font-size="11" text-anchor="middle">weight</text>
<line x1="332" y1="100" x2="332" y2="184" stroke="#12A594" stroke-width="1.8" marker-end="url(#bw-up)"/>
<text x="332" y="206" fill="#12A594" font-size="11" text-anchor="middle">buoyancy</text>
<text x="300" y="226" fill="currentColor" font-size="11.5" text-anchor="middle" opacity="0.65">Buoyancy = weight of the water pushed aside (ρ·g·V) — it floats when buoyancy balances weight.</text>
</svg>

---

## The Principle

Around 250 BCE, Archimedes observed that an object submerged in a fluid experiences an upward force equal to the weight of the fluid it displaces. Written as a relationship rather than a formula:

$$\text{Buoyant force} = \text{weight of displaced fluid}$$

For a floating body in equilibrium the buoyant force exactly equals the body's own weight. The body sinks just deep enough that the water it pushes aside weighs as much as it does. If you push it deeper, the displaced water weighs more than the object and it rises back. If you push it shallower, the object outweighs the displaced water and it sinks again. Equilibrium is self-correcting.

A steel ship floats because its hull is hollow. The total weight of the steel shell, machinery, cargo, and fuel is spread over a large displaced volume — a volume whose equivalent water-weight equals that total. The density of the combined *ship-plus-enclosed-air* system is less than water, so it floats.

---

## Seawater Density — the One Constant that Connects Weight to Volume

Salt water at 15 °C has a density of approximately 1.025 tonnes per cubic metre. That number appears at the top of `src/analysis.c` as a named constant:

```c
/* from src/analysis.c */
#define SEAWATER_DENSITY    1.025f  /* t/m3 at 15C */
```

It is the conversion factor between *mass* (how heavy the ship is) and *volume* (how much water must be moved aside to keep it afloat). Given a displacement in tonnes:

$$V_{displaced} = \frac{\Delta}{\rho_{SW}} = \frac{\Delta}{1.025}$$

where $\Delta$ is displacement in tonnes and $V$ is in cubic metres. This is the only physical constant hard-coded into the analysis; every other number is either measured (from the hydrostatic table) or derived from it.

!!! note "Fresh water vs. salt water"
    Fresh water is 1.000 t/m³. A ship in a fresh-water river sits slightly deeper than in open sea because fresh water is less dense — the same displaced volume weighs less, so the hull must go deeper to displace more of it. CargoForge-C uses the salt-water constant throughout.

---

## From Weight to Displacement to Draft

`perform_analysis` (in `src/analysis.c`) builds displacement by summing all mass contributions:

```c
/* from src/analysis.c, perform_analysis() */
float displacement_kg = ship->lightship_weight + r.total_cargo_weight_kg;

/* Include tank weight in displacement if tanks are loaded */
float tank_weight_t = 0.0f;
if (ship->tanks && ship->tanks->count > 0) {
    tank_weight_t = calculate_tank_weight(ship->tanks);
    displacement_kg += tank_weight_t * 1000.0f;
}
```

Then it converts to tonnes and finds the displaced volume:

```c
float displacement_t  = displacement_kg / 1000.0f;
float displaced_vol   = displacement_t / SEAWATER_DENSITY;
```

With the displaced volume known, the question becomes: at what draft does the hull enclose that volume? The answer depends on the hull's shape.

### Box-Hull Fallback

When no hydrostatic table is loaded (`ship->hydro` is NULL), CargoForge-C approximates the hull as a rectangular box scaled by a block coefficient:

```c
/* from src/analysis.c, box-hull branch */
r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
```

The block coefficient `BLOCK_COEFF = 0.75` accounts for the fact that real ship hulls are not perfect boxes — they taper at bow and stern. A value of 0.75 means the actual underwater volume is 75% of the bounding rectangular box of the same length, width, and draft. So the hull must go a little deeper than a pure box would to displace the required volume.

### Table-Based Hydrostatics

Real ships are measured in dry-dock at many drafts and the resulting data is published in a hydrostatic table. `src/hydrostatics.c` reads that table from a CSV file and provides two look-up functions:

- `hydro_draft_from_displacement` — given a displacement in tonnes, finds the matching draft by searching the table and interpolating between rows.
- `hydro_interpolate` — given a draft, returns all hydrostatic properties (KB, BM, TPC, MTC, …) at that draft.

Both use a simple linear interpolation helper:

```c
/* from src/hydrostatics.c */
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}
```

When a displacement falls between two table rows, `t` is the fractional position within that interval, and every field of the output `HydroEntry` is blended proportionally. This is exactly the same mathematics as reading between ruled lines on a graph.

---

## KB — Where Is the Centre of Buoyancy?

The buoyant force does not act at the waterline; it acts at the centroid of the displaced volume, called the **centre of buoyancy (B)**. Its height above the keel is written $KB$.

For the box-hull approximation, $KB$ is estimated as a fixed fraction of the draft:

```c
r.kb = KB_FACTOR * r.draft;   /* KB_FACTOR = 0.53 */
```

For a uniform rectangular cross-section, the centroid of the submerged rectangle would be exactly at half the draft ($KB = 0.5 \times T$). Real hull forms are fuller near the keel and taper upward, so their centroid is slightly higher — the constant 0.53 is a well-established approximation for typical cargo ships.

When a hydrostatic table is available, KB is read directly from the interpolated row:

```c
r.kb = he.kb;   /* from HydroEntry at interpolated draft */
```

---

## BM — Metacentric Radius and the Source of Initial Stability

Archimedes' principle explains *that* a ship floats; it does not immediately explain *why* it rights itself when tilted. That second question is answered by the **metacentre**.

When a ship heels by a small angle, the waterplane (the cross-section at the waterline) rotates. One side dips deeper, the other rises. The displaced volume shifts to the lower side, moving the centre of buoyancy B sideways. This sideward shift generates a righting couple — as long as the new position of B is above G (the centre of gravity). The height of this "virtual pivot point" above B is called $BM$, the **metacentric radius**:

$$BM = \frac{I_T}{V}$$

where $I_T$ is the second moment of area of the waterplane about its centreline (a measure of how wide the waterplane is) and $V$ is the displaced volume.

For the box-hull fallback, CargoForge-C computes this directly:

```c
/* from src/analysis.c, box-hull branch */
float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
r.bm = inertia_t / displaced_vol;
```

The $L \times B^3 / 12$ factor is the second moment of area of a rectangle about its long centreline. `WATERPLANE_COEFF = 0.85` reduces it to account for the real waterplane being slightly smaller than the bounding rectangle. When a hydrostatic table is used, BM comes directly from the measured `he.bm`.

!!! tip "Why width cubed?"
    Beam (width) appears cubed in $I_T = L B^3 / 12$. This means doubling the beam multiplies BM by eight. Wider ships are dramatically more stable in initial heel — which is why loaded barges can look absurdly stable and narrow racing yachts are tender.

---

## GM — Putting It All Together

With KB and BM known, the transverse metacentric height GM is:

$$GM = KB + BM - KG$$

In `perform_analysis`:

```c
/* from src/analysis.c */
r.gm = r.kb + r.bm - r.kg;
```

A positive GM means the metacentre M is above the centre of gravity G — the ship is stable and will right itself. A negative GM means G is above M — the ship will capsize. The IMO sets a minimum of 0.15 m (the constant `IMO_GM_MIN` in `src/analysis.c`). CargoForge-C uses `gm_corrected` — GM after subtracting any free-surface correction — for all subsequent stability criteria.

| Symbol | Meaning | Source (box-hull) | Source (table) |
|--------|---------|-------------------|---------------|
| $KB$ | Height of centre of buoyancy | $0.53 \times T$ | Interpolated from CSV |
| $BM$ | Metacentric radius | $I_T / V$ | Interpolated from CSV |
| $KG$ | Height of centre of gravity | Moment sum / displacement | Same |
| $GM$ | Metacentric height | $KB + BM - KG$ | Same |

---

## The Hydrostatic Table in Practice

The CSV format expected by `parse_hydro_table` in `src/hydrostatics.c` requires at least seven comma-separated columns per row in ascending draft order:

```
# draft_m, displacement_t, km_m, kb_m, bm_m, tpc_t_per_cm, mtc_tm_per_cm
4.0,  8500,  6.80, 2.12, 4.68, 18.5, 142.3
4.5,  9800,  6.75, 2.38, 4.37, 19.1, 148.7
5.0, 11200,  6.71, 2.65, 4.06, 19.8, 155.2
```

`parse_hydro_table` enforces strictly ascending drafts — if a row's draft is not greater than the previous row's, parsing aborts with an error. This matters because `hydro_draft_from_displacement` relies on monotonically increasing displacement as well; non-monotonic tables would silently produce wrong interpolations.

!!! warning "Table boundaries"
    Both interpolation functions clamp to the nearest boundary row when the requested draft or displacement falls outside the table range. They do not extrapolate. If a fully-loaded ship's displacement exceeds the table's heaviest entry, CargoForge-C returns the hydrostatic values at maximum tabulated draft — a conservative approximation, but not a crash.

---

## Recap

- Archimedes' principle: a floating body displaces a volume of water whose weight exactly equals the body's own weight. A hollow steel ship floats because its average density (hull plus air) is less than seawater.
- CargoForge-C uses `SEAWATER_DENSITY = 1.025 t/m³` to convert displacement (tonnes) to displaced volume (m³): $V = \Delta / 1.025$.
- Draft is found from displaced volume either via the box-hull formula (using `BLOCK_COEFF = 0.75`) or by inverse interpolation in a hydrostatic CSV table loaded through `src/hydrostatics.c`.
- KB (height of centre of buoyancy) and BM (metacentric radius, $I_T / V$) come from the same table row or from box-hull constants.
- All three feed into the fundamental stability equation $GM = KB + BM - KG$, computed in `perform_analysis` in `src/analysis.c`.
- IMO requires $GM \geq 0.15\ \text{m}$; CargoForge-C enforces this as part of its six-criterion intact stability check.

*Next: [Displacement, density, and draft](18-displacement-density-draft.md).*
