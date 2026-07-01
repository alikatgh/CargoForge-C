# The center of buoyancy and the metacenter

A ship floats because the water it displaces pushes back. Where exactly that push acts — and how it shifts when the ship tilts — determines whether the vessel rights itself or capsizes. This lesson explains the geometry behind buoyancy: KB (the vertical center of buoyancy), BM (the metacentric radius), and the metacenter M — the pivot point that [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) computes before every stability check.

## The mental model 🧠

The centre of buoyancy B is the balance point of *the water the hull pushes aside* — and unlike the centre of gravity, it refuses to stay put. Tip the ship and the underwater shape changes: more hull plunges in on the low side, less on the high side, so B slides toward the side that went under.

Now draw the upward buoyancy force as a line through that shifted B and extend it up to the ship's centreline. The point where it crosses is the **metacentre M** — and here is the magic: for small angles M barely moves, so the heeling hull effectively swings about M like a pendulum hung from a fixed pivot. The height of that pivot above B is `BM = I / V` — the very waterplane inertia from the box-hull lesson, which is why beam (cubed in `I`) buys stability so cheaply. M is what turns "B wandered off-centre" into a predictable restoring geometry, and it is the middle term of the equation everything rides on: `GM = KB + BM − KG`.

<svg viewBox="0 0 600 280" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:520px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The metacentre M: when the hull heels, the buoyancy line through the shifted B crosses the centreline at M</title>
<desc>As the ship heels, the centre of buoyancy moves from B on the centreline to B prime on the low side, because more hull is immersed there. The upward buoyancy force through B prime, extended to the ship's centreline, crosses it at the metacentre M. The height of M above B is the metacentric radius BM. For small angles M stays nearly fixed, so the hull swings about it like a pendulum.</desc>
<path d="M248,148 Q254,244 286,245 Q330,246 360,148" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.3" stroke-width="1"/>
<line x1="272" y1="240" x2="272" y2="86" stroke="currentColor" stroke-opacity="0.3" stroke-dasharray="4 3"/>
<text x="272" y="80" font-size="9" text-anchor="middle" fill="currentColor" opacity="0.45">upright</text>
<line x1="272" y1="239" x2="312" y2="64" stroke="currentColor" stroke-opacity="0.75" stroke-width="1.4"/>
<text x="244" y="150" font-size="9" text-anchor="middle" fill="currentColor" opacity="0.5" transform="rotate(-13 244 150)">centreline</text>
<path d="M272,212 A30,30 0 0,1 281,214" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<text x="288" y="210" font-size="10" fill="currentColor" opacity="0.7">θ</text>
<line x1="312" y1="212" x2="312" y2="70" stroke="#12A594" stroke-width="1.6"/><path d="M307,77 L312,66 L317,77" fill="#12A594"/>
<text x="320" y="200" font-size="10" fill="#12A594">buoyancy ↑</text>
<line x1="290" y1="161" x2="309" y2="161" stroke="#D05663" stroke-opacity="0.7" stroke-dasharray="3 2"/>
<text x="300" y="150" font-size="8.5" text-anchor="middle" fill="#D05663" opacity="0.9">B shifts to low side</text>
<circle cx="312" cy="64" r="3.2" fill="#12A594"/><text x="320" y="62" font-size="11" fill="currentColor">M — metacentre</text>
<circle cx="290" cy="161" r="3" fill="currentColor"/><text x="272" y="166" font-size="11" text-anchor="end" fill="currentColor">B</text>
<circle cx="312" cy="161" r="3" fill="#D05663"/><text x="318" y="173" font-size="11" fill="#D05663">B′</text>
<circle cx="272" cy="239" r="3" fill="currentColor"/><text x="264" y="252" font-size="11" text-anchor="end" fill="currentColor">K</text>
<text x="318" y="116" font-size="10" fill="currentColor" opacity="0.7">BM</text>
<text x="300" y="272" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">GM = KB + BM − KG  ·  M must stay above G for the ship to self-right</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **KB (vertical centre of buoyancy)** = "how high up the ship's underwater push is acting" — `perform_analysis` estimates this as `KB_FACTOR * draft` (0.53 × the depth underwater) for a typical cargo hull, or reads it from a hydrostatic table when one is loaded.
- **BM (metacentric radius)** = "how much the ship's stability cushion grows with beam" — computed in `analysis.c` as waterplane inertia divided by displaced volume (`inertia_t / displaced_vol`); because beam is cubed in the formula, a wider ship gets a dramatically bigger BM for free.
- **M (metacenter)** = "the pivot point the ship rocks around for small rolls" — it is not a physical fitting on the vessel, just a geometric point; `perform_analysis` locates it with the single line `r.gm = r.kb + r.bm - r.kg`.
- **GM** = "the safety margin between where the ship rocks and where its weight sits" — positive means the ship pushes back upright; negative means it will capsize; this number is what every call to `perform_analysis` ultimately produces.
- **Hydrostatic table vs. box-hull fallback** = "measured hull data vs. a good approximation" — when `ship->hydro->loaded == 1`, `hydrostatics.c` does a two-step lookup (displacement → draft via `hydro_draft_from_displacement`, then draft → KB/BM via `hydro_interpolate`); otherwise fixed coefficients (`KB_FACTOR = 0.53`, `WATERPLANE_COEFF = 0.85`) stand in.
- **Waterplane second moment of area (I_T)** = "how spread-out the ship's footprint at the waterline is" — the wider the ship, the larger I_T, the larger BM, and the more resistant it is to rolling; this is why cargo ships are built wide rather than tall.

**Why it matters:** if KB or BM is wrong, GM is wrong, and a ship declared stable may capsize at sea; getting the two-mode calculation (`hydro_table_used = 0` vs `1`) correct is the difference between a planning estimate and a result fit for classification-society submission.

---

## Why geometry matters

Place a brick in a bathtub. The water level rises. The weight of water displaced exactly equals the weight of the brick (Archimedes' principle). But a ship is not a brick — it is hollow and asymmetric, and it can roll. Stability asks: *when it rolls by a small angle, does the net force push it back upright, or push it further over?*

The answer depends on the relative positions of two points:

| Symbol | Name | What it is |
|--------|------|------------|
| G | Centre of gravity | Where the ship's total weight acts (downward) |
| B | Centre of buoyancy | Centroid of the displaced volume (upward thrust acts here) |
| M | Metacenter | The point B travels toward as the ship heels; stability pivot |

When M is above G, the ship is stable. When M is below G, it capsizes. The vertical distance $GM$ is the single most important number in stability analysis.

---

## KB — the vertical centre of buoyancy

B is the geometric centroid of the submerged volume. For a flat-bottomed box hull floating upright, B sits at exactly half the draft. Real hulls are wider in the middle and narrower at the keel, so B sits a little higher than the halfway point.

### The box-hull approximation

`analysis.c` defines a constant at the top of the file:

```c
// from src/analysis.c
#define KB_FACTOR  0.53f  /* KB ~ 0.53*T for typical cargo ships */
```

When no hydrostatic table is loaded, `perform_analysis` estimates KB as:

```c
// from src/analysis.c (lines 173-174)
r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
r.kb = KB_FACTOR * r.draft;
```

The constant 0.53 encodes the fact that the centroid of a ship-shaped cross-section sits at about 53 % of the draft — slightly above the midpoint because the hull is fuller near the waterline than near the keel.

### Reading KB from a hydrostatic table

When a hydrostatic CSV is loaded (`ship->hydro->loaded == 1`), KB is measured for the actual hull at every draft and interpolated from the table:

```c
// from src/analysis.c (lines 152-157)
r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);

HydroEntry he;
hydro_interpolate(ship->hydro, r.draft, &he);

r.kb = he.kb;
r.bm = he.bm;
```

`hydro_interpolate` finds the two table rows that bracket the current draft and performs linear interpolation across all fields at once — including `kb` and `bm`:

```c
// from src/hydrostatics.c (lines 126-134)
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
```

The `interpolate_entries` helper applies `lerp` to every field of `HydroEntry`:

```c
// from src/hydrostatics.c (lines 24-35)
static void interpolate_entries(const HydroEntry *lo, const HydroEntry *hi,
                                float t, HydroEntry *out) {
    out->draft        = lerp(lo->draft,        hi->draft,        t);
    out->kb           = lerp(lo->kb,           hi->kb,           t);
    out->bm           = lerp(lo->bm,           hi->bm,           t);
    // ... and all other fields
}
```

!!! note "Why clamp at the table boundaries?"
    `hydro_interpolate` returns the nearest row when the requested draft falls outside the table range. Extrapolating hydrostatic curves beyond measured data introduces errors that can exceed 10 %. Clamping is the conservative choice — and the code is explicit about it (lines 116–123 of `hydrostatics.c`).

---

## BM — the metacentric radius

When a ship heels by a small angle $\theta$, the submerged volume shifts to the low side: more volume is submerged to starboard if the ship rolls to starboard. B moves with it. For small angles, B traces an arc whose radius of curvature is called BM:

$$BM = \frac{I_T}{V}$$

where:

- $I_T$ is the **second moment of area of the waterplane** about the ship's centreline (m⁴)
- $V$ is the **displaced volume** (m³)

### What is the waterplane second moment of area?

Imagine cutting the ship horizontally at the waterline. The resulting shape is the waterplane — roughly an oval. The second moment of area $I_T$ measures how that area is distributed away from the centreline:

$$I_T = \int y^2 \, dA$$

Wide beams give large $I_T$. For a perfect rectangle of length $L$ and beam $B$:

$$I_T = \frac{L B^3}{12}$$

The beam enters as a cube, so doubling the beam increases BM eightfold. This is why ships are wide, not tall.

### The box-hull computation in `analysis.c`

```c
// from src/analysis.c (lines 175-177)
/* BM: transverse metacentric radius = I_T / V */
float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
r.bm = inertia_t / displaced_vol;
```

`WATERPLANE_COEFF` (0.85) scales the rectangle down to account for the fact that real waterplanes are not perfect rectangles — they narrow at bow and stern:

```c
// from src/analysis.c (line 23)
#define WATERPLANE_COEFF  0.85f  /* Cw: waterplane area / L*B */
```

So the effective second moment is:

$$I_T \approx C_w \cdot \frac{L B^3}{12}$$

and therefore:

$$BM \approx \frac{C_w \cdot L B^3 / 12}{V}$$

When using a hydrostatic table, `bm` is read directly from the `HydroEntry` — the naval architect who computed the table already integrated the actual hull geometry precisely.

---

## M — the metacenter

M is defined as the point where the vertical line through B (the buoyancy force line) intersects the ship's centreline when the ship heels by a small angle. For small heels, M stays in one place — it is a property of the waterplane geometry and the displacement, not of the heel angle itself.

The height of M above the keel (KM) is simply:

$$KM = KB + BM$$

CargoForge-C stores KB and BM separately so that GM can be computed for any cargo configuration without re-measuring the hull geometry:

$$GM = KB + BM - KG$$

This is the one line that brings the whole lesson together, and it appears verbatim in `perform_analysis`:

```c
// from src/analysis.c (line 185)
r.gm = r.kb + r.bm - r.kg;
```

!!! warning "Positive GM is not the whole story"
    A positive GM means the ship will resist *small* heels. IMO regulations also require that GZ (the righting lever at large angles) stays large enough over 0–40°. The next lessons cover the GZ curve and the free-surface correction that reduces the effective GM when tanks are partially filled.

---

## From displacement to draft — inverse interpolation

Before KB and BM can be read from the table, the code must know the draft that corresponds to the current displacement. Displacement is known (it is the sum of all weights). Draft is not. `hydrostatics.c` solves this by inverse-interpolating on the displacement column:

```c
// from src/hydrostatics.c (lines 154-162)
for (int i = 0; i < table->count - 1; i++) {
    float disp_lo = table->entries[i].displacement;
    float disp_hi = table->entries[i + 1].displacement;

    if (displacement_t >= disp_lo && displacement_t <= disp_hi) {
        float range = disp_hi - disp_lo;
        float t = (range > 1e-6f) ? (displacement_t - disp_lo) / range : 0.0f;
        return lerp(table->entries[i].draft, table->entries[i + 1].draft, t);
    }
}
```

The fraction `t` locates the displacement between two table rows; the same fraction is applied to the draft column. Once the draft is known, `hydro_interpolate` is called to get KB, BM, and all other parameters at that draft.

!!! tip "Two-step lookup"
    The process is always: displacement → draft (inverse interpolation via `hydro_draft_from_displacement`) → KB and BM (forward interpolation via `hydro_interpolate`). The code in `perform_analysis` (lines 151–156 of `analysis.c`) follows this exact sequence.

---

## Two modes side by side

| | Box-hull fallback | Hydrostatic table |
|---|---|---|
| **Trigger** | `ship->hydro == NULL` or not loaded | `ship->hydro->loaded == 1` |
| **Draft** | $V / (L \cdot B \cdot C_b)$ | Inverse interpolation on displacement column |
| **KB** | $0.53 \times \text{draft}$ | Read from `HydroEntry.kb` |
| **BM** | $C_w \cdot L B^3 / (12 V)$ | Read from `HydroEntry.bm` |
| **Accuracy** | Adequate for early planning | Required for classification-society submission |
| **Reported as** | `hydro_table_used = 0` | `hydro_table_used = 1` |

The human-readable output from `print_loading_plan` reports which mode was used:

```
Hydrostatics: Table interpolation
```
or
```
Hydrostatics: Box-hull approximation
```

---

## Recap

- **B** is the centroid of the displaced volume; its vertical position KB is proportional to draft (box-hull) or read from a CSV table (table mode).
- **$BM = I_T / V$**: the waterplane second moment of area divided by displaced volume. Beam enters as a cube — wide ships are inherently more stable.
- **M** (the metacenter) is the pivot point for small heels. Its height above the keel is $KM = KB + BM$.
- **$GM = KB + BM - KG$**: positive means self-righting; computed in `perform_analysis` as `r.gm = r.kb + r.bm - r.kg`.
- When a hydrostatic table is loaded, `hydrostatics.c` performs a two-step lookup — inverse interpolation to find draft from displacement, then forward interpolation to read KB and BM at that draft.
- The box-hull fallback uses fixed coefficients (`KB_FACTOR = 0.53`, `WATERPLANE_COEFF = 0.85`) and is appropriate for design exploration, not final certification.

## Check yourself

??? question "Why does the centre of buoyancy B move when a ship heels, but the centre of gravity G does not?"
    B is the centroid of the underwater hull shape, which genuinely changes as more hull submerges on the low side and less on the high side. G is the centroid of the ship's fixed weight distribution — tilting the hull doesn't move any weight, so G stays put.

??? question "What's special about the metacentre M for small heel angles?"
    It stays nearly fixed in place even as B swings around beneath it, so the heeling hull behaves like a pendulum hung from a stationary pivot at M. That stability of M is exactly what makes GM a dependable safety margin.

*Next: [GM: why ships self-right](23-gm-why-ships-self-right.md).*
