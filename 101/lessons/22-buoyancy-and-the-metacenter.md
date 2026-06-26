# The center of buoyancy and the metacenter

A ship floats because the water it displaces pushes back. Where exactly that push acts — and how it shifts when the ship tilts — determines whether the vessel rights itself or capsizes. This lesson explains the geometry behind buoyancy: KB (the vertical center of buoyancy), BM (the metacentric radius), and the metacenter M — the pivot point that [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) computes before every stability check.

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

*Next: [GM: why ships self-right](23-gm-why-ships-self-right.md).*
